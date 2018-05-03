#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#define other(x)        ((x) ^ 1)
#define real_player(x)  ((x) & 1)
#define C4_NONE          2
#define pop_state()     (current_state = &state_stack[--depth])
#define C4_MAX_LEVEL    20
#define goodness_of(player)  (current_state->score[player] - current_state->score[other(player)])

enum { HUMAN = 0, COMPUTER = 1 };

static int get_num(char *prompt, int lower, int upper, int default_val);
static void print_board(int width, int height);
static void print_dot(void);
static char piece[2] = { 'X', 'O' };

void    c4_poll(void(*poll_func)(void), clock_t interval);
void    c4_new_game(int width, int height, int num);
bool    c4_make_move(int player, int column, int *row);
bool    c4_auto_move(int player, int level, int *column, int *row);
char ** c4_board(void);
int     c4_score_of_player(int player);
bool    c4_is_winner(int player);
bool    c4_is_tie(void);
void    c4_win_coords(int *x1, int *y1, int *x2, int *y2);
void    c4_end_game(void);
void    c4_reset(void);

static int size_x, size_y, total_size;
static int num_to_connect;
static int win_places;

static int ***map;

typedef struct {

	char **board;
	int *(score_array[2]);
	int score[2];
	short int winner;
	int num_of_pieces;
} Game_state;

static int magic_win_number;
static bool game_in_progress = false, move_in_progress = false;
static bool seed_chosen = false;
static void(*poll_function)(void) = NULL;
static clock_t poll_interval, next_poll;
static Game_state state_stack[C4_MAX_LEVEL + 1];
static Game_state *current_state;
static int depth;
static int states_allocated = 0;
static int *drop_order;

static int num_of_win_places(int x, int y, int n);
static void update_score(int player, int x, int y);
static int drop_piece(int player, int column);
static void push_state(void);
static int evaluate(int player, int level, int alpha, int beta);
static int rule(int player);
static void *emalloc(size_t size);


const char *c4_get_version(void);

int
main()
{
	int player[2], level[2], turn = 0, num_of_players, move;
	int width, height, num_to_connect;
	int x1, y1, x2, y2;
	char buffer[80];

	width = 7;
	height = 6;
	num_to_connect = 4;

	num_of_players = 1;

	player[0] = HUMAN;
	player[1] = COMPUTER;
	level[1] = 10;
	buffer[0] = '\0';
	do {
		printf("Would you like to go first [y]? ");
		if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
			printf("\nGoodbye!\n");
			exit(0);
		}
		buffer[0] = tolower(buffer[0]);
	} while (buffer[0] != 'y' && buffer[0] != 'n' && buffer[0] != '\n');

	turn = (buffer[0] == 'n') ? 1 : 0;

	c4_new_game(width, height, num_to_connect);
	c4_poll(print_dot, CLOCKS_PER_SEC / 2);

	do {//형진이가 수정할 곳, 입력값은 착수점과 Search Algorithm(1) Rule(2), 2 선택시 어떤 rule 적용했는지도 출력 미친거 아냐
		print_board(width, height);
		if (player[turn] == HUMAN) {
			do {
				if (num_of_players == 2)
					sprintf(buffer, "Player %c, drop in which column",
						piece[turn]);
				else
					sprintf(buffer, "Drop in which column");
				move = get_num(buffer, 1, width, -1) - 1;
			} while (!c4_make_move(turn, move, NULL));
		}
		else {
			if (num_of_players == 1)
				printf("Thinking.");
			else
				printf("Player %c is thinking.", piece[turn]);
			fflush(stdout);
			c4_auto_move(turn, level[turn], &move, NULL);
			if (num_of_players == 1)
				printf("\n\nI dropped my piece into column %d.\n", move + 1);
			else
				printf("\n\nPlayer %c dropped its piece into column %d.\n",
					piece[turn], move + 1);
		}

		turn = !turn;

	} while (!c4_is_winner(0) && !c4_is_winner(1) && !c4_is_tie());

	print_board(width, height);

	if (c4_is_winner(0)) {
		if (num_of_players == 1)
			printf("You won!");
		else
			printf("Player %c won!", piece[0]);
		c4_win_coords(&x1, &y1, &x2, &y2);
		printf("  (%d,%d) to (%d,%d)\n\n", x1 + 1, y1 + 1, x2 + 1, y2 + 1);
	}
	else if (c4_is_winner(1)) {
		if (num_of_players == 1)
			printf("I won!");
		else
			printf("Player %c won!", piece[1]);
		c4_win_coords(&x1, &y1, &x2, &y2);
		printf("  (%d,%d) to (%d,%d)\n\n", x1 + 1, y1 + 1, x2 + 1, y2 + 1);
	}
	else {
		printf("There was a tie!\n\n");
	}

	c4_end_game();
	return 0;
}


static void
print_board(int width, int height)
{
	int x, y;
	char **board, spacing[2], dashing[2];

	board = c4_board();

	spacing[1] = dashing[1] = '\0';
	if (width > 19) {
		spacing[0] = '\0';
		dashing[0] = '\0';
	}
	else {
		spacing[0] = ' ';
		dashing[0] = '-';
	}

	printf("\n");
	for (y = height - 1; y >= 0; y--) {

		printf("|");
		for (x = 0; x<width; x++) {
			if (board[x][y] == C4_NONE)
				printf("%s %s|", spacing, spacing);
			else
				printf("%s%c%s|", spacing, piece[(int)board[x][y]], spacing);
		}
		printf("\n");

		printf("+");
		for (x = 0; x<width; x++)
			printf("%s-%s+", dashing, dashing);
		printf("\n");
	}

	printf(" ");
	for (x = 0; x<width; x++)
		printf("%s%d%s ", spacing, (x>8) ? (x + 1) / 10 : x + 1, spacing);
	if (width > 9) {
		printf("\n ");
		for (x = 0; x<width; x++)
			printf("%s%c%s ", spacing, (x>8) ? '0' + (x + 1) - ((x + 1) / 10) * 10 : ' ',
				spacing);
	}
	printf("\n\n");
}

/****************************************************************************/

static void
print_dot(void)
{
	printf(".");
	fflush(stdout);
}

void
c4_poll(void(*poll_func)(void), clock_t interval)
{
	poll_function = poll_func;
	poll_interval = interval;
}

void
c4_new_game(int width, int height, int num)
{
	register int i, j, k, x;
	int win_index, column;
	int *win_indices;

	assert(!game_in_progress);
	assert(width >= 1 && height >= 1 && num >= 1);

	size_x = width;
	size_y = height;
	total_size = width * height;
	num_to_connect = num;
	magic_win_number = 1 << num_to_connect;
	win_places = num_of_win_places(size_x, size_y, num_to_connect);

	if (!seed_chosen) {
		srand((unsigned int)time((time_t *)0));
		seed_chosen = true;
	}

	depth = 0;
	current_state = &state_stack[0];

	current_state->board = (char **)emalloc(size_x * sizeof(char *));
	for (i = 0; i<size_x; i++) {
		current_state->board[i] = (char *)emalloc(size_y);
		for (j = 0; j<size_y; j++)
			current_state->board[i][j] = C4_NONE;
	}
	current_state->score_array[0] = (int *)emalloc(win_places * sizeof(int));
	current_state->score_array[1] = (int *)emalloc(win_places * sizeof(int));
	for (i = 0; i<win_places; i++) {
		current_state->score_array[0][i] = 1;
		current_state->score_array[1][i] = 1;
	}

	current_state->score[0] = current_state->score[1] = win_places;
	current_state->winner = C4_NONE;
	current_state->num_of_pieces = 0;

	states_allocated = 1;

	/* Set up the map */

	map = (int ***)emalloc(size_x * sizeof(int **));
	for (i = 0; i<size_x; i++) {
		map[i] = (int **)emalloc(size_y * sizeof(int *));
		for (j = 0; j<size_y; j++) {
			map[i][j] = (int *)emalloc((num_to_connect * 4 + 1) * sizeof(int));
			map[i][j][0] = -1;
		}
	}

	win_index = 0;

	/* Fill in the horizontal win positions */
	for (i = 0; i<size_y; i++)
		for (j = 0; j<size_x - num_to_connect + 1; j++) {
			for (k = 0; k<num_to_connect; k++) {
				win_indices = map[j + k][i];
				for (x = 0; win_indices[x] != -1; x++)
					;
				win_indices[x++] = win_index;
				win_indices[x] = -1;
			}
			win_index++;
		}

	/* Fill in the vertical win positions */
	for (i = 0; i<size_x; i++)
		for (j = 0; j<size_y - num_to_connect + 1; j++) {
			for (k = 0; k<num_to_connect; k++) {
				win_indices = map[i][j + k];
				for (x = 0; win_indices[x] != -1; x++)
					;
				win_indices[x++] = win_index;
				win_indices[x] = -1;
			}
			win_index++;
		}

	/* Fill in the forward diagonal win positions */
	for (i = 0; i<size_y - num_to_connect + 1; i++)
		for (j = 0; j<size_x - num_to_connect + 1; j++) {
			for (k = 0; k<num_to_connect; k++) {
				win_indices = map[j + k][i + k];
				for (x = 0; win_indices[x] != -1; x++)
					;
				win_indices[x++] = win_index;
				win_indices[x] = -1;
			}
			win_index++;
		}

	/* Fill in the backward diagonal win positions */
	for (i = 0; i<size_y - num_to_connect + 1; i++)
		for (j = size_x - 1; j >= num_to_connect - 1; j--) {
			for (k = 0; k<num_to_connect; k++) {
				win_indices = map[j - k][i + k];
				for (x = 0; win_indices[x] != -1; x++)
					;
				win_indices[x++] = win_index;
				win_indices[x] = -1;
			}
			win_index++;
		}

	drop_order = (int *)emalloc(size_x * sizeof(int));
	column = (size_x - 1) / 2;
	for (i = 1; i <= size_x; i++) {
		drop_order[i - 1] = column;
		column += ((i % 2) ? i : -i);
	}

	game_in_progress = true;
}

bool
c4_make_move(int player, int column, int *row)
{
	assert(game_in_progress);
	assert(!move_in_progress);

	if (column >= size_x || column < 0)
		return false;

	int result = drop_piece(real_player(player), column);
	if (row != NULL && result >= 0)
		*row = result;
	return (result >= 0);
}


bool
c4_auto_move(int player, int level, int *column, int *row)
{
	int best_column = -1, goodness = 0, best_worst = -(INT_MAX);
	int num_of_equal = 0, real_player, current_column, result;

	assert(game_in_progress);
	assert(!move_in_progress);
	assert(level >= 1 && level <= C4_MAX_LEVEL);

	real_player = real_player(player);

	if (current_state->num_of_pieces < 2 &&
		size_x == 7 && size_y == 6 && num_to_connect == 4 &&
		(current_state->num_of_pieces == 0 ||
			current_state->board[3][0] != C4_NONE)) {
		if (column != NULL)
			*column = 3;
		if (row != NULL)
			*row = current_state->num_of_pieces;
		drop_piece(real_player, 3);
		return true;
	}

	move_in_progress = true;

	for (int i = 0; i<size_x; i++) {
		push_state();
		current_column = drop_order[i];

		result = drop_piece(real_player, current_column);

		if (result < 0) {
			pop_state();
			continue;
		}

		else if (current_state->winner == real_player) {
			best_column = current_column;
			pop_state();
			break;
		}

		else {
			next_poll = clock() + poll_interval;
			goodness = evaluate(real_player, level, -(INT_MAX), -best_worst);
		}

		if (goodness > best_worst) {
			best_worst = goodness;
			best_column = current_column;
			num_of_equal = 1;
		}


		else if (goodness == best_worst) {
			num_of_equal++;
			if ((rand() >> 4) % num_of_equal == 0)
				best_column = current_column;
		}

		pop_state();
	}

	move_in_progress = false;

	if (best_column >= 0) {
		result = drop_piece(real_player, best_column);
		if (column != NULL)
			*column = best_column;
		if (row != NULL)
			*row = result;
		return true;
	}
	else
		return false;
}

char **
c4_board(void)
{
	assert(game_in_progress);
	return current_state->board;
}

int
c4_score_of_player(int player)
{
	assert(game_in_progress);
	return current_state->score[real_player(player)];
}

bool
c4_is_winner(int player)
{
	assert(game_in_progress);
	return (current_state->winner == real_player(player));
}

bool
c4_is_tie(void)
{
	assert(game_in_progress);
	return (current_state->num_of_pieces == total_size &&
		current_state->winner == C4_NONE);
}

void
c4_win_coords(int *x1, int *y1, int *x2, int *y2)
{
	register int i, j, k;
	int winner, win_pos = 0;
	bool found;

	assert(game_in_progress);

	winner = current_state->winner;
	assert(winner != C4_NONE);

	while (current_state->score_array[winner][win_pos] != magic_win_number)
		win_pos++;

	found = false;
	for (j = 0; j<size_y && !found; j++)
		for (i = 0; i<size_x && !found; i++)
			for (k = 0; map[i][j][k] != -1; k++)
				if (map[i][j][k] == win_pos) {
					*x1 = i;
					*y1 = j;
					found = true;
					break;
				}

	found = false;
	for (j = size_y - 1; j >= 0 && !found; j--)
		for (i = size_x - 1; i >= 0 && !found; i--)
			for (k = 0; map[i][j][k] != -1; k++)
				if (map[i][j][k] == win_pos) {
					*x2 = i;
					*y2 = j;
					found = true;
					break;
				}
}

void
c4_end_game(void)
{
	int i, j;

	assert(game_in_progress);
	assert(!move_in_progress);

	for (i = 0; i<size_x; i++) {
		for (j = 0; j<size_y; j++)
			free(map[i][j]);
		free(map[i]);
	}
	free(map);

	for (i = 0; i<states_allocated; i++) {
		for (j = 0; j<size_x; j++)
			free(state_stack[i].board[j]);
		free(state_stack[i].board);
		free(state_stack[i].score_array[0]);
		free(state_stack[i].score_array[1]);
	}
	states_allocated = 0;


	free(drop_order);

	game_in_progress = false;
}

void
c4_reset(void)
{
	assert(!move_in_progress);
	if (game_in_progress)
		c4_end_game();
	poll_function = NULL;
}

const char *
c4_get_version(void)
{
	return "$Id: c4.c,v 3.11 2009/11/03 14:42:01 pomakis Exp pomakis $";
}


static int
num_of_win_places(int x, int y, int n)
{
	if (x < n && y < n)
		return 0;
	else if (x < n)
		return x * ((y - n) + 1);
	else if (y < n)
		return y * ((x - n) + 1);
	else
		return 4 * x*y - 3 * x*n - 3 * y*n + 3 * x + 3 * y - 4 * n + 2 * n*n + 2;
}

static int
get_num(char *prompt, int lower, int upper, int default_value)
{
	int number = -1;
	int result;
	static char numbuf[40];

	do {
		if (default_value != -1)
			printf("%s [%d]? ", prompt, default_value);
		else
			printf("%s? ", prompt);

		if (fgets(numbuf, sizeof(numbuf), stdin) == NULL || numbuf[0] == 'q') {
			printf("\nGoodbye!\n");
			exit(0);
		}
		result = sscanf(numbuf, "%d", &number);
	} while (result == 0 || (result != EOF && (number<lower || number>upper)));

	return ((result == EOF) ? default_value : number);
}
static void
update_score(int player, int x, int y)
{
	register int i;
	int win_index;
	int this_difference = 0, other_difference = 0;
	int **current_score_array = current_state->score_array;
	int other_player = other(player);

	for (i = 0; map[x][y][i] != -1; i++) {
		win_index = map[x][y][i];
		this_difference += current_score_array[player][win_index];
		other_difference += current_score_array[other_player][win_index];

		current_score_array[player][win_index] <<= 1;
		current_score_array[other_player][win_index] = 0;

		if (current_score_array[player][win_index] == magic_win_number)
			if (current_state->winner == C4_NONE)
				current_state->winner = player;
	}

	current_state->score[player] += this_difference;
	current_state->score[other_player] -= other_difference;
}

static int
drop_piece(int player, int column)
{
	int y = 0;

	while (current_state->board[column][y] != C4_NONE && ++y < size_y)
		;

	if (y == size_y)
		return -1;

	current_state->board[column][y] = player;
	current_state->num_of_pieces++;
	update_score(player, column, y);

	return y;
}

static void
push_state(void)
{
	register int i, win_places_array_size;
	Game_state *old_state, *new_state;

	win_places_array_size = win_places * sizeof(int);
	old_state = &state_stack[depth++];
	new_state = &state_stack[depth];

	if (depth == states_allocated) {

		/* Allocate space for the board */

		new_state->board = (char **)emalloc(size_x * sizeof(char *));
		for (i = 0; i<size_x; i++)
			new_state->board[i] = (char *)emalloc(size_y);

		/* Allocate space for the score array */

		new_state->score_array[0] = (int *)emalloc(win_places_array_size);
		new_state->score_array[1] = (int *)emalloc(win_places_array_size);

		states_allocated++;
	}

	/* Copy the board */

	for (i = 0; i<size_x; i++)
		memcpy(new_state->board[i], old_state->board[i], size_y);

	/* Copy the score array */

	memcpy(new_state->score_array[0], old_state->score_array[0],
		win_places_array_size);
	memcpy(new_state->score_array[1], old_state->score_array[1],
		win_places_array_size);

	new_state->score[0] = old_state->score[0];
	new_state->score[1] = old_state->score[1];
	new_state->winner = old_state->winner;
	new_state->num_of_pieces = old_state->num_of_pieces;

	current_state = new_state;
}

static int
evaluate(int player, int level, int alpha, int beta)
{
	if (poll_function != NULL && next_poll <= clock()) {
		next_poll += poll_interval;
		(*poll_function)();
	}

	if (current_state->winner == player)
		return INT_MAX - depth;
	else if (current_state->winner == other(player))
		return -(INT_MAX - depth);
	else if (current_state->num_of_pieces == total_size)
		return 0; /* a tie */
	else if (level == depth)
		return goodness_of(player);
	else {
		/* Assume it is the other player's turn. */
		int best = -(INT_MAX);
		int maxab = alpha;
		for (int i = 0; i<size_x; i++) {
			if (current_state->board[drop_order[i]][size_y - 1] != C4_NONE)
				continue; /* The column is full. */
			push_state();
			drop_piece(other(player), drop_order[i]);
			int goodness = evaluate(other(player), level, -beta, -maxab);
			if (goodness > best) {
				best = goodness;
				if (best > maxab)
					maxab = best;
			}
			pop_state();
			if (best > beta)
				break;
		}

		/* What's good for the other player is bad for this one. */
		return -best;
	}
}
//리턴값은 착수할 column 값
static int
rule(int player) {
	int value[7];
	int current_column;
	int another_player = 1 - player;
	int result;

	for (int k = 0; k<7; k++)
		value[k] = 0;
	int value_int = 0;

	//민이 파트
	for (int i = 0; i<7; i++) {
		push_state();
		current_column = drop_order[i];
		result = drop_piece(player, current_column);
		//승리인 경우 바로 돌을 놓고 게임 끝내기
		if (current_state->winner == player) {
			pop_state();
			return i;
		}
		//다음 수에 상대방이 승리?
		for (int j = 0; j<7; j++) {
			push_state();
			current_column = drop_order[j];
			result = drop_piece(another_player, current_column);
			if (current_state->winner == another_player) {
				value[i] -= 1000;
				pop_state();
				break;
			}
			pop_state();
		}
		//7자 모양이 존재하게 될(가로줄 대각선줄이 모두 존재할 수 있게 하는) 경우 100점
		if ((current_column<7 - 2) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column][result - 2] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column + 2][result] ==
					current_state->board[current_column + 1][result - 1] ==
					current_state->board[current_column + 2][result - 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column<7 - 1) && (current_column>0) && (result>2)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column - 1][result - 2] == player) ||
				(current_state->board[current_column - 1][result] ==
					current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column][result - 1] ==
					current_state->board[current_column + 1][result - 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>1) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result] ==
					current_state->board[current_column - 2][result] ==
					current_state->board[current_column - 1][result - 1] ==
					current_state->board[current_column][result - 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>0) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column - 1][result - 1] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result + 1] ==
					current_state->board[current_column][result + 1] ==
					current_state->board[current_column + 1][result + 1] ==
					current_state->board[current_column + 1][result - 1] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>0) && (current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 2] ==
				current_state->board[current_column + 1][result + 2] ==
				current_state->board[current_column + 2][result + 2] ==
				current_state->board[current_column + 1][result + 1] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 2] ==
				current_state->board[current_column - 1][result + 2] ==
				current_state->board[current_column - 2][result + 2] ==
				current_state->board[current_column - 1][result + 1] == player)) {
				value[i] += 100;
			}
		}

		if ((current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column][result + 2] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column + 2][result] ==
					current_state->board[current_column + 1][result + 1] ==
					current_state->board[current_column + 2][result + 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column<7 - 1) && (current_column>0) && (result<size_y - 2)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column - 1][result + 2] == player) ||
				(current_state->board[current_column - 1][result] ==
					current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column][result + 1] ==
					current_state->board[current_column + 1][result + 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result] ==
					current_state->board[current_column - 2][result] ==
					current_state->board[current_column - 1][result + 1] ==
					current_state->board[current_column][result + 2] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>0) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column - 1][result + 1] == player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result - 1] ==
					current_state->board[current_column][result - 1] ==
					current_state->board[current_column + 1][result - 1] ==
					current_state->board[current_column + 1][result + 1] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>0) && (current_column<7 - 2) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 2] ==
				current_state->board[current_column + 1][result - 2] ==
				current_state->board[current_column + 2][result - 2] ==
				current_state->board[current_column + 1][result - 1] == player)) {
				value[i] += 100;
			}
		}
		if ((current_column>1) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 2] ==
				current_state->board[current_column - 1][result - 2] ==
				current_state->board[current_column - 2][result - 2] ==
				current_state->board[current_column - 1][result - 1] == player)) {
				value[i] += 100;
			}
		}
		//상대방의 가로 또는 대각선으로 세 개의 돌이 연속할 수 있는 경우를 저지하게 될 경우 80점
		if ((current_column<7 - 2) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column][result - 2] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column + 2][result] ==
					current_state->board[current_column + 1][result - 1] ==
					current_state->board[current_column + 2][result - 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column<7 - 1) && (current_column>0) && (result>2)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column - 1][result - 2] != player) ||
				(current_state->board[current_column - 1][result] ==
					current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column][result - 1] ==
					current_state->board[current_column + 1][result - 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>1) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result] ==
					current_state->board[current_column - 2][result] ==
					current_state->board[current_column - 1][result - 1] ==
					current_state->board[current_column][result - 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>0) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column - 1][result - 1] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result + 1] ==
					current_state->board[current_column][result + 1] ==
					current_state->board[current_column + 1][result + 1] ==
					current_state->board[current_column + 1][result - 1] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>0) && (current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 2] ==
				current_state->board[current_column + 1][result + 2] ==
				current_state->board[current_column + 2][result + 2] ==
				current_state->board[current_column + 1][result + 1] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 2] ==
				current_state->board[current_column - 1][result + 2] ==
				current_state->board[current_column - 2][result + 2] ==
				current_state->board[current_column - 1][result + 1] != player)) {
				value[i] += 80;
			}
		}

		if ((current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column][result + 2] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column + 2][result] ==
					current_state->board[current_column + 1][result + 1] ==
					current_state->board[current_column + 2][result + 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column<7 - 1) && (current_column>0) && (result<size_y - 2)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column - 1][result + 2] != player) ||
				(current_state->board[current_column - 1][result] ==
					current_state->board[current_column][result] ==
					current_state->board[current_column + 1][result] ==
					current_state->board[current_column][result + 1] ==
					current_state->board[current_column + 1][result + 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result] ==
					current_state->board[current_column - 2][result] ==
					current_state->board[current_column - 1][result + 1] ==
					current_state->board[current_column][result + 2] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>0) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column - 1][result + 1] != player) ||
				(current_state->board[current_column][result] ==
					current_state->board[current_column - 1][result - 1] ==
					current_state->board[current_column][result - 1] ==
					current_state->board[current_column + 1][result - 1] ==
					current_state->board[current_column + 1][result + 1] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>0) && (current_column<7 - 2) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 2] ==
				current_state->board[current_column + 1][result - 2] ==
				current_state->board[current_column + 2][result - 2] ==
				current_state->board[current_column + 1][result - 1] != player)) {
				value[i] += 80;
			}
		}
		if ((current_column>1) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 2] ==
				current_state->board[current_column - 1][result - 2] ==
				current_state->board[current_column - 2][result - 2] ==
				current_state->board[current_column - 1][result - 1] != player)) {
				value[i] += 80;
			}
		}
		//가로 또는 대각선으로 세 개의 돌이 연속하여 존재하게 될 경우 60점
		//가로
		if ((current_column<7 - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column<7 - 1) && (current_column>0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column - 1][result] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column<7) && (current_column>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] == player)) {
				value[i] += 60; //-1 -2 아닐까
			}
		}
		// 대각선
		if ((current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column + 2][result + 2] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>1) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column - 1][result - 1] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column>1) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] == player)) {
				value[i] += 60;
			}
		}

		// 반대 대각선
		if ((current_column<7 - 2) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column + 2][result - 2] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>1) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column - 1][result + 1] == player)) {
				value[i] += 60;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] == player)) {
				value[i] += 60;
			}
		}
		//상대방의 가로나 대각선 저지
		//가로
		if ((current_column<7 - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column<7 - 1) && (current_column>0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column - 1][result] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column<7) && (current_column>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] != player)) {
				value[i] += 50;
			}
		}
		// /대각선
		if ((current_column<7 - 2) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column + 2][result + 2] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>1) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result + 1] ==
				current_state->board[current_column - 1][result - 1] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column>1) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] != player)) {
				value[i] += 50;
			}
		}

		// 반대 대각선
		if ((current_column<7 - 2) && (result>2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column + 2][result - 2] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column>0) && (current_column<7 - 1) && (result>1) && (result<size_y - 1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column + 1][result - 1] ==
				current_state->board[current_column - 1][result + 1] != player)) {
				value[i] += 50;
			}
		}
		if ((current_column>1) && (result<size_y - 2)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] != player)) {
				value[i] += 50;
			}
		}

		// 소영이 파트
		//상대방의 세로로 세 개의 돌이 연속할 수 있는 경우를 저지하게 될 경우 40점
		if ((result<4)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column][result + 2] != player)) {
				value[i] += 40;
			}
		}

		if ((result<5) && (result>0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column][result + 1] != player)) {
				value[i] += 40;
			}
		}

		if ((result<6) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column][result - 2] != player)) {
				value[i] += 40;
			}
		}

		//두개의 돌이 가로로 붙어 있고, 아래쪽이 벽면인 경우의 좌우 40점
		if ((current_column<6)&&(result==0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column+1][result] == player)) {
				value[i] += 40;
			}
		}

		if ((current_column>0) && (current_column<7)&&(result==0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column - 1][result] == player)) {
				value[i] += 40;
			}
		}

		//세로로 세 개의 돌이 연속하여 존재하게 될 경우  10점
		if ((result<4)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result + 1] ==
				current_state->board[current_column][result + 2] == player)) {
				value[i] += 10;
			}
		}

		if ((result<5) && (result>0)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column][result + 1] == player)) {
				value[i] += 10;
			}
		}

		if ((result<6) && (result>1)) {
			if ((current_state->board[current_column][result] ==
				current_state->board[current_column][result - 1] ==
				current_state->board[current_column][result - 2] == player)) {
				value[i] += 10;
			}
		}

		//가로, 대각선으로 이을수 있는 4칸의 여유가 있을 경우 5점 
		//가로
		if ((current_column < 4)) {
			if ((current_state->board[current_column+1][result] ==
				current_state->board[current_column+2][result] ==
				current_state->board[current_column+3][result] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column < 7)&&(current_column>2)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 3][result] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column < 5)&&(current_column>0)) {
			if ((current_state->board[current_column - 1][result] ==
				current_state->board[current_column + 1][result] ==
				current_state->board[current_column + 2][result] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column < 6)&&(current_column>3)) {
			if ((current_state->board[current_column - 2][result] ==
				current_state->board[current_column - 1][result] ==
				current_state->board[current_column + 1][result] == NULL)) {
				value[i] += 5;
			}
		}
		//오른쪽 위 대각선
		if ((current_column<4) && (result<3)) {
			if ((current_state->board[current_column+1][result+1] ==
				current_state->board[current_column + 2][result + 2] ==
				current_state->board[current_column +3][result +3] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<7) && (current_column>2) && (result<6) && (result>2)) {
			if ((current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] ==
				current_state->board[current_column - 3][result - 3] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<6) && (current_column>1) && (result<5) && (result>1)) {
			if ((current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] ==
				current_state->board[current_column +1][result +1] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<5) && (current_column>0) && (result<4) && (result>0)) {
			if ((current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column + 2][result + 2] ==
				current_state->board[current_column +1][result +1] == NULL)) {
				value[i] += 5;
			}
		}
		//왼쪽 위 대각선
		if ((current_column < 4) && (result>2)) {
			if ((current_state->board[current_column - 1][result - 1] ==
				current_state->board[current_column - 2][result - 2] ==
				current_state->board[current_column - 3][result - 3] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<7) && (current_column>2) && (result<4)) {
			if ((current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] ==
				current_state->board[current_column - 3][result + 3] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<5) && (current_column>0) && (result<5) && (result>1)) {
			if ((current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column + 2][result - 2] ==
				current_state->board[current_column + 1][result - 1] == NULL)) {
				value[i] += 5;
			}
		}

		if ((current_column<6) && (current_column>1) && (result<4) && (result>0)) {
			if ((current_state->board[current_column - 1][result + 1] ==
				current_state->board[current_column - 2][result + 2] ==
				current_state->board[current_column + 1][result - 1] == NULL)) {
				value[i] += 5;
			}
		}
		//상대방이 내가 둘 수 위에 두어 승리할 수 있는 경우 두지 않음 -200점 ????????

		for (int j = 0; j<size_y; j++) { //size_y=7
			push_state();
			result = drop_order[j];
			current_column = drop_piece(another_player, current_column);
			if (current_state->winner == another_player) {
				value[i] -= 200;
				pop_state();
				break;
			}
			pop_state();
		}

		pop_state();
	}


	for (int k = 1; k<7; k++) {
		if (value[k]>value[value_int])
			value_int = k;
	}
	return value;
}

static void *
emalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL) {
		fprintf(stderr, "c4: emalloc() - Can't allocate %ld bytes.\n",
			(long)size);
		exit(1);
	}
	return ptr;
}
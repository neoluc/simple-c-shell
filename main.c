#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#define INPUT_SIZE 1024
#define MAX_TOKENS_SIZE 100
#define MAX_ARGUMENT_NUMBER 100
#define MAX_COMMAND_NUMBER 10
#define MAX_FILE_NUMBER 100

struct Job {
	int index;
	char descriptions[100];
	int pids[MAX_COMMAND_NUMBER];
	int pids_size;
	char flag;
	struct Job *next;
	struct Job *previous;
};

struct Job_List {
	struct Job *head;
	struct Job *tail;
};

struct Job_List job_list = {NULL, NULL};

int job_list_push(int pids[], int pids_size, char descriptions[], char *error_message) {

	struct Job *job = malloc(sizeof(struct Job));

	job->index = (job_list.tail == NULL)? 1: job_list.tail->index + 1;

	strcpy(job->descriptions, descriptions);

	int i;
	for (i = 0; i < pids_size; i++) {
		job->pids[i] = pids[i];
	}
	job->pids_size = pids_size;

	job->flag = '+';

	job->next = NULL;
	job->previous = NULL;

	struct Job *change_job = job_list.head;
	while (change_job) {
		if (change_job->flag == '+') {
			change_job->flag = '-';
		} else if (change_job->flag == '-') {
			change_job->flag = ' ';
		}
		change_job = change_job->next;
	}

	if (job_list.tail == NULL) {
		job_list.head = job;
		job_list.tail = job;
	} else {
		job->previous = job_list.tail;
		job_list.tail->next = job;
		job_list.tail = job;
	}

	return 0;
}

int job_list_insert(int index, int pids[], int pids_size, char descriptions[], char *error_message) {

	struct Job *job = malloc(sizeof(struct Job));

	job->index = index;
	strcpy(job->descriptions, descriptions);
	int i;
	for (i = 0; i < pids_size; i++) {
		job->pids[i] = pids[i];
	}
	job->pids_size = pids_size;

	job->flag = '+';

	job->next = NULL;
	job->previous = NULL;

	struct Job *change_job = job_list.head;
	while (change_job) {
		if (change_job->flag == '+') {
			change_job->flag = '-';
		} else if (change_job->flag == '-') {
			change_job->flag = ' ';
		}
		change_job = change_job->next;
	}

	if (job_list.tail == NULL) {
		job_list.head = job;
		job_list.tail = job;
		return 0;
	}

	struct Job *job_after = job_list.head;
	while (job_after && job->index < index) {
		job_after = job->next;
	}

	if (job_after->index == index) {
		strcpy(error_message, "index already exist");
		return -1;
	}

	if (job_after->index < index) {
		job_after->next = job;
		job->previous = job_after;
		job_list.tail = job;
		return 0;
	}

	if (job_after == job_list.head) {
		job_after->previous = job;
		job->next = job_after;
		job_list.head = job;
		return 0;
	}

	job->next = job_after;
	job->previous = job_after->previous;
	job_after->previous->next = job;
	job_after->previous = job;
	return 0;
}

int job_list_remove(struct Job *job, char *error_message) {

	if (job == job_list.head && job == job_list.tail) {
		job_list.head = NULL;
		job_list.tail = NULL;
	
	} else if (job == job_list.head) {
		job_list.head = job->next;
		job_list.head->previous = NULL;
	
	} else if (job == job_list.tail) {
		job_list.tail = job->previous;
		job_list.tail->next = NULL;
	
	} else {
		job->next->previous = job->previous;
		job->previous->next = job->next;
	}

	free(job);

	return 0;
}

enum File_Redirect_Type {
	REDIRECT_OUTPUT,
	APPEND_OUTPUT,
	REDIRECT_INPUT,
};

struct File {
	char *name;
	enum File_Redirect_Type redirect_type;
};

struct Command {
	char *arguments[MAX_ARGUMENT_NUMBER];
	int arguments_size;
	struct File files[MAX_FILE_NUMBER];
	int files_size;
};

struct Built_In_Command {
	char *name;
	int (*execute_command)(struct Command, char *);
};

int _execute_cd(struct Command command, char *error_message) {
	if (chdir(command.arguments[1] == NULL? getenv("HOME"): command.arguments[1]) == -1) {
		strcpy(error_message, "fail in change directory");
		return -1;
	}
	return 0;
}

int _execute_exit(struct Command command, char *error_message) {

	if (job_list.tail != NULL) {
		printf("There are stopped jobs.\n");
		return 0;
	}
	
	exit(EXIT_SUCCESS);

	return 0;
}

int _execute_history(struct Command command, char *error_message) {

	return 0;
}

int _execute_fg(struct Command command, char *error_message) {

	int i;
	for (i = 1; command.arguments[i] != NULL || i == 1; i++) {
		char argument[100];
		if (command.arguments[i] == NULL) {
			if (job_list.tail == NULL) {
				strcpy(argument, "current");
			} else {
				sprintf(argument, "%d", job_list.tail->index);
			}
		} else {
			strcpy(argument, command.arguments[i]);
		}
		int is_matched = 0;
		struct Job *job = job_list.head;
		while (job) {
			if (job->index == atoi(argument)) {
				is_matched = 1;
				int j;
				for (j = 0; j < job->pids_size; j++) {
					if (kill(job->pids[j], SIGCONT) == -1) {
						strcpy(error_message, "Error in kill");
						return -1;
					}
				}

				int index = job->index;
				char descriptions[100];
				strcpy(descriptions, job->descriptions);
				int pids[MAX_COMMAND_NUMBER];
				for (j = 0; j < job->pids_size; j++) {
					pids[j] = job->pids[j];
				}
				int pids_size = job->pids_size;
				
				if (job_list_remove(job, error_message) < 0) {
					return -1;
				}

				int statuses[MAX_COMMAND_NUMBER];
				for (j = 0; j < job->pids_size; j++) {
					if (waitpid(pids[j], &statuses[j], WUNTRACED) != pids[j]) {
						strcpy(error_message, "Error in waitpid in parent");
						return -1;
					}
				}

				int is_process_stopped = 0;
				for (j = 0; j < job->pids_size; j++) {
					if (WIFSTOPPED(statuses[j])) {
						is_process_stopped = 1;
					}
				}

				if (is_process_stopped) {
					if (job_list_insert(index, pids, pids_size, descriptions, error_message) < 0) {
						return -1;
					}
					printf("[%d]+  Stopped                 %s\n", index, descriptions);
				}
			}
			job = job->next;
		}
		if (!is_matched) {
			printf("%s: no such job\n", argument);
		}
	}

	return 0;
}

int _execute_jobs(struct Command command, char *error_message) {

	char arguments[MAX_ARGUMENT_NUMBER][100];
	int arguments_size = 0;

	int i;
	for (i = 1; command.arguments[i] != NULL; i++) {
		strcpy(arguments[arguments_size], command.arguments[i]);
		arguments_size++;
	}

	if (arguments_size == 0) {
		struct Job *job = job_list.head;
		while (job) {
			sprintf(arguments[arguments_size], "%d", job->index);
			arguments_size++;
			job = job->next;
		}
	}

	for (i = 0; i < arguments_size; i++) {
		int is_matched = 0;
		struct Job *job = job_list.head;
		while (job) {
			if (atoi(arguments[i]) == job->index) {
				is_matched = 1;
				printf("[%d]%c  Stopped                 %s\n", job->index, job->flag, job->descriptions);
			}
			job = job->next;
		}
		if (!is_matched) {
			printf("%s: no such job\n", arguments[i]);
		}
	}

	return 0;
}

const struct Built_In_Command built_in_commands[] = {
	{"cd", _execute_cd},
	{"exit", _execute_exit},
	{"history", _execute_history},
	{"jobs", _execute_jobs},
	{"fg", _execute_fg},
};

const int built_in_commands_size = sizeof(built_in_commands)/sizeof(*built_in_commands);

int is_built_in_command(char *command_name) {
	int i;
	for (i = 0; i < built_in_commands_size; i++) {
		if (strcmp(command_name, built_in_commands[i].name) == 0) {
			return 1;
		}
	}

	return 0;
}

int execute_built_in_command(struct Command command, char *error_message) {
	int i;
	for (i = 0; i < built_in_commands_size; i++) {
		if (strcmp(command.arguments[0], built_in_commands[i].name) == 0) {
			if (built_in_commands[i].execute_command(command, error_message) < 0) {
				return -1;
			}
			return 0;
		}
	}

	strcpy(error_message, "no such built in command");
	return -1;
}

int parse_input(char *tokens[], int tokens_size, struct Command commands[], int *number_command, char *error_message) {
	
	char invalid_characters[] = {32, 9, 62, 60, 124, 42, 33, 96, 39, 34};

	enum Token_State {
		WAIT_COMMAND,
		WAIT_ARGUMENT,
		WAIT_FILE,
		WAIT_MORE,
	};

	enum Token_State state = WAIT_COMMAND;

	*number_command = 0;

	int i;

	for (i = 0; i < tokens_size; i++) {

		switch ((int)state) {
		case WAIT_COMMAND:

			commands[*number_command].arguments_size = 0;
			commands[*number_command].arguments_size = 0;

			// if (strcspn(tokens[i], invalid_characters) < strlen(tokens[i])) {
			// 	error = 1;
			// }

			state = WAIT_ARGUMENT;
			
			commands[*number_command].arguments[commands[*number_command].arguments_size] = tokens[i];
			commands[*number_command].arguments_size++;

			break;
		
		case WAIT_ARGUMENT:

			// if (strcspn(tokens[i], invalid_characters) < strlen(tokens[i])) {
			// 	error = 1;
			// }

			if (strcmp(tokens[i], "|") == 0) {
				state = WAIT_COMMAND;
				commands[*number_command].arguments[commands[*number_command].arguments_size] = NULL;
				(*number_command)++;

			} else if (strcmp(tokens[i], "<") == 0) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = REDIRECT_INPUT;
				state = WAIT_FILE;

			} else if (strcmp(tokens[i], ">") == 0) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = REDIRECT_OUTPUT;
				state = WAIT_FILE;

			} else if (strcmp(tokens[i], ">>") == 0) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = APPEND_OUTPUT;
				state = WAIT_FILE;

			} else {
				commands[*number_command].arguments[commands[*number_command].arguments_size] = tokens[i];
				commands[*number_command].arguments_size++;
			}

			break;

		case WAIT_FILE:

			commands[*number_command].files[commands[*number_command].files_size].name = tokens[i];
			commands[*number_command].files_size++;

			state = WAIT_MORE;	

			break;

		case WAIT_MORE:

			if (strcmp(tokens[i], "|") == 0) {
				state = WAIT_COMMAND;	
				commands[*number_command].arguments[commands[*number_command].arguments_size] = NULL;
				(*number_command)++;

			} else if (strcmp(tokens[i], "<") == 0 ) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = REDIRECT_INPUT;
				state = WAIT_FILE;

			} else if (strcmp(tokens[i], ">") == 0 ) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = REDIRECT_OUTPUT;
				state = WAIT_FILE;

			} else if (strcmp(tokens[i], ">>") == 0) {
				commands[*number_command].files[commands[*number_command].files_size].redirect_type = APPEND_OUTPUT;
				state = WAIT_FILE;
			}

			break;

		}

	}

	if (state == WAIT_FILE || state == WAIT_COMMAND) {
		strcpy(error_message, "Syntax error");
		return -1;
	}

	commands[*number_command].arguments[commands[*number_command].arguments_size] = NULL;
	commands[*number_command].files[commands[*number_command].files_size].name = NULL;

	(*number_command)++;

	return 0;
}

int _split_string(char *string, char *delimiter, char *tokens[], int *tokens_size, char *error_message) {
	int string_size = strlen(string);
	int delimiter_size = strlen(delimiter);
	if (delimiter_size == 0) {
		strcpy(error_message, "delimiter size is zero");
		return -1;
	}

	int i;
	int positions[100];
	int positions_size = 0;
	positions[positions_size++] = 0;
	for (i = 0; i < string_size; i++) {
		if (string[i] == delimiter[0]) {
			int j;
			for (j = 0; j < delimiter_size && i + j < string_size && string[i + j] == delimiter[j]; j++);
			if (j == delimiter_size) {
				positions[positions_size++] = i;
				positions[positions_size++] = i + delimiter_size;
				i += delimiter_size - 1;
			}
		}
	}
	positions[positions_size++] = string_size;

	*tokens_size = 0;
	for (i = 1; i < positions_size; i++) {
		if (positions[i] > positions[i - 1]) {
			tokens[*tokens_size] = malloc((positions[i]-positions[i - 1] + 1) * sizeof(char));
			if (tokens[*tokens_size] == NULL) {
				strcpy(error_message, "Error in malloc in split string");
				return -1;
			}
			int j;
			int k;
			for (j=positions[i - 1], k = 0; j < positions[i]; j++, k++) {
				tokens[*tokens_size][k] = string[j];
			}
			tokens[*tokens_size][k] = '\0';
			(*tokens_size)++;
		}
	}

	return 0;
}

int split_input_into_tokens(char *input, char *tokens[], int *tokens_size, char *error_message) {

	char *delimiters[] = {"<", ">>", ">", "|", " "};
	int delimiters_size = sizeof(delimiters)/sizeof(*delimiters);

	char *temp[MAX_TOKENS_SIZE];
	int temp_size = 0;

	temp[0] = malloc((strlen(input) + 1)*sizeof(char));
	if (temp[0] == NULL) {
		strcpy(error_message, "Error in malloc in split input into tokens");
		return -1;
	}
	strcpy(temp[0], input);
	temp_size = 1;

	char *swap[MAX_TOKENS_SIZE];
	int swap_size = 0;
	int i;
	for (i = 0; i<delimiters_size; i++) {
		int j;
		for (j = 0; j < temp_size; j++) {
			int is_delimiter = 0;
			int k;
			for (k=0; k<delimiters_size; k++) {
				if (strcmp(temp[j], delimiters[k]) == 0) {
					is_delimiter = 1;
					break;
				}
			}
			if (is_delimiter) {
				swap[swap_size] = malloc((strlen(temp[j]) + 1)*sizeof(char));
				if (swap[swap_size] == NULL) {
					strcpy(error_message, "Error in malloc in split input into tokens");
					return -1;
				}
				strcpy(swap[swap_size++], temp[j]);
			} else {
				char *each[MAX_TOKENS_SIZE];
				int each_size = 0;
				if (_split_string(temp[j], delimiters[i], each, &each_size, error_message) < 0) {
					return -1;
				}
				for (k = 0; k < each_size; k++) {
					swap[swap_size++] = each[k];
				}
			}
			free(temp[j]);
		}
		for (j = 0; j < swap_size; j++) {
			temp[j] = swap[j];
		}
		temp_size = swap_size;
		swap_size = 0;
	}

	*tokens_size = 0;
	for (i = 0; i < temp_size; i++) {
		if (strcmp(temp[i], " ") != 0) {
			tokens[*tokens_size] = temp[i];
			(*tokens_size)++;
		} else {
			free(temp[i]);
		}
	}

	return 0;
}

int process_commands(struct Command commands[], int number_command, char *tokens[], int tokens_size, char *error_message) {

	int pids[MAX_COMMAND_NUMBER];
	int fds[MAX_COMMAND_NUMBER][2];
	int statuses[MAX_COMMAND_NUMBER];

	int number_pipe = number_command - 1;

	int i;
	for (i = 0; i < number_pipe; i++) {
		if (pipe(fds[i]) == -1 ) {
			strcpy(error_message, "Error in pipe");
			return -1;
		}
	}

	for (i = 0; i < number_command; i++) {

		if ((pids[i] = fork()) == -1) {
			strcpy(error_message, "Error in fork");
			return -1;
		}

		if (pids[i] == 0) {

			signal(SIGINT,SIG_DFL);
			signal(SIGTERM,SIG_DFL);
			signal(SIGQUIT,SIG_DFL);
			signal(SIGTSTP,SIG_DFL);
			signal(SIGSTOP,SIG_DFL);
			signal(SIGKILL,SIG_DFL);

			if (i != 0 && dup2(fds[i - 1][0], STDIN_FILENO) == -1) {
				strcpy(error_message, "Error in dup2 stdin");
				return -1;
			}

			if (i != number_command - 1 && dup2(fds[i][1], STDOUT_FILENO) == -1) {
				strcpy(error_message, "Error in dup2 stdout");
				return -1;
			}

			int j;
			for (j = 0; j < number_pipe; j++) {
				if (close(fds[j][0]) == -1 || close(fds[j][1]) == -1) {
					strcpy(error_message, "Error in closing file description");
					return -1;
				}
			}

			for (j = 0; commands[i].files[j].name != NULL; j++) {
				int redirect_fd;
				int old_fd;

				switch (commands[i].files[j].redirect_type) {
			
				case REDIRECT_OUTPUT:
					old_fd = STDOUT_FILENO;
					redirect_fd = open(commands[i].files[j].name, O_TRUNC | O_WRONLY | O_CREAT, S_IRWXU);
					break;

				case APPEND_OUTPUT:
					old_fd = STDOUT_FILENO;
					redirect_fd = open(commands[i].files[j].name, O_APPEND | O_WRONLY | O_CREAT, S_IRWXU);
					break;

				case REDIRECT_INPUT:
					old_fd = STDIN_FILENO;
					redirect_fd = open(commands[i].files[j].name, O_RDONLY);
					break;
				}

				if (redirect_fd == -1) {
					strcpy(error_message, "Error in opening redirect file");
					return -1;
				}

				if (dup2(redirect_fd, old_fd) == -1) {
					strcpy(error_message, "Error in dup2 redirect file");
					return -1;
				}

				if (close(redirect_fd) == -1) {
					strcpy(error_message, "Error in close redirect file");
					return -1;
				}
			}

			if (is_built_in_command(commands[i].arguments[0])) {
				if (execute_built_in_command(commands[i], error_message) < 0) {
					return -1;
				}
			} else {
				if (execvp(commands[i].arguments[0], commands[i].arguments) == -1) {
					strcpy(error_message, "Error in execvp");
					return -1;
				}			
			}

			exit(EXIT_SUCCESS);
		}
	}

	for (i = 0; i < number_pipe; i++) {
		if (close(fds[i][0]) == -1 || close(fds[i][1]) == -1) {
			strcpy(error_message, "Error in closing file description");
			return -1;
		}
	}

	for (i = 0; i < number_command; i++) {
		if (waitpid(pids[i], &statuses[i], WUNTRACED) != pids[i]) {
			strcpy(error_message, "Error in waitpid in parent");
			return -1;
		}
	}

	int is_process_stopped = 0;
	for (i = 0; i < number_command; i++) {
		if (WIFSTOPPED(statuses[i])) {
			is_process_stopped = 1;
		}
	}
	if (is_process_stopped) {
		char job_descriptions[100] = "";
		for (i = 0; i < tokens_size; i++) {
			if (i != 0) {
				strcat(job_descriptions, " ");
			}
			strcat(job_descriptions, tokens[i]);
		}
		if (job_list_push(pids, number_command, job_descriptions, error_message) < 0) {
			return -1;
		}
		printf("[%d]+  Stopped                 %s\n", job_list.tail->index, job_list.tail->descriptions);
	}



	return 0;
}

int main(int argc, char **argv) {
	
	setenv("PATH", "/bin:/bin/usr:.", 1);

	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGSTOP, SIG_DFL);
	signal(SIGKILL, SIG_DFL);

	while (1) {

		char cwd[1024];
		char input[1024];
		char *tokens[256];
		int tokens_size;
		struct Command commands[MAX_COMMAND_NUMBER];
		char error_message[256];

		getcwd(cwd, sizeof(cwd)/sizeof(*cwd));

		printf("shell:%s$ ", cwd);

		if (fgets(input, sizeof(input)/sizeof(*input), stdin) == NULL) {
			printf("\n");
			break;
		}

		int input_size = strlen(input);
		if (input[input_size-1] == '\n') {
			input[input_size-1] = '\0';
		}

		if (strcmp(input, "") == 0) {
			continue;
		}

		if (split_input_into_tokens(input, tokens, &tokens_size, error_message) < 0) {
			printf("%s\n", error_message);
			continue;
		}

		int number_command;

		if (parse_input(tokens, tokens_size, commands, &number_command, error_message) < 0) {
			printf("%s\n", error_message);
			continue;
		}

		if (number_command == 1 && is_built_in_command(commands[0].arguments[0])) {
			if (execute_built_in_command(commands[0], error_message) < 0) {
				printf("%s\n", error_message);
			}
			continue;
		}

		if (process_commands(commands, number_command, tokens, tokens_size, error_message) < 0) {
			printf("%s\n", error_message);
			continue;
		}

		int i;
		for (i = 0; i < tokens_size; i++) {
			free(tokens[i]);
		}
	}

	return 0;
}

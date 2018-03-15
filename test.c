#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <fcntl.h>

typedef struct
{
	char **argv;
	int fdIn, fdOut;
}Command;

//adding comment

void deleteNL(char **);
int count_pipes(char *);
int tokenizer(char **, Command *);
void run_pipe(Command *, int, int);
void redirect(int, int);
void closeFd(int fd);

static int version = 10;

void displayVersion(){
	printf("Version: %d\n", version);
}

int main(int argc, char **argv)
{
	displayVersion();
	const int cmd_limit = 1024;
	char *user_input = malloc(sizeof(char) * BUFSIZ);
	int num_pipes, num_cmds;
	Command cmds[cmd_limit];

	while(fgets(user_input, sizeof(char) * BUFSIZ, stdin) != NULL)		{
		//delete the new line character at the end of input, count pipes, then tokenize usr input
		deleteNL(&user_input);
		num_pipes = count_pipes(user_input);

		num_cmds = tokenizer(&user_input, cmds);

		//piping
		run_pipe(cmds, num_cmds, num_pipes);

		//reset input for next line of input
		memset(user_input, 0, sizeof(user_input));
		fflush(NULL);

		//free command arguments
		int i = 0;
		for(i; i < num_cmds; i++){
		free(cmds[i].argv);
		}
	}
	free(user_input);
}


//counts the number of pipes
int count_pipes(char * input){
	int pipe_count = 0;
	for(int i = 0; input[i] != '\0'; i++)
	{
		if(input[i] == '|')
		{
			pipe_count++;
		}
	}
	printf("Pipe count: %d\n", pipe_count);
	return pipe_count;
}

//delete the new line character at the end of user input

void deleteNL(char ** input){
	if((*input)[strlen(*input) - 1] == '\n'){
		(*input)[strlen(*input) - 1] = '\0';
	}
}

//tokenizes user input command, opens any files found in the command. After tokenizing, the tokens are made ready for execvp() and added to Command structure that will be used to execute the commands in pipes
int tokenizer(char ** input, Command * cmds)
{
	const char *DELIMS = " ";
	char **tokenArg = malloc(sizeof(char) * BUFSIZ); //temp token to store tokens before adding to command agr token
	char *token;
	int i = 0, j = 0; //counters loops and indexs
	int cmd_after_pipe = 0, file_after_r = 0, command_after_semi = 0; //flags for catching commands after pipes and semicolons, and file after redirection
       	int is_in_r = 0, is_out_r = 0, is_append = 0; //recognizes input or output file redirection and append to file

	int num_of_commands = 1, num_tokens = 0; //first one counts comman, there is always at least one command

	token = strtok(*input, DELIMS);
	while(token != NULL)
		{
		switch(token[0])
         {
			case '-':
				{
					printf("Options: %s\n", token);
					break;
				}
			case '|':
				{
					num_of_commands++; //once we hit a pipe, we have finished a command
					cmd_after_pipe = 1; // set flag to indicate we found a pipe
					break;
				}
			case '<':
				{
					printf("Redirection: %s\n", token);
					file_after_r = 1;
					is_in_r = 1;
					token = strtok(NULL, DELIMS);
					continue; 	//skip input re token
				}
			case '>':
				{
					printf("Redirection: %s\n", token);

					//check if we are appending
					if(token[1] && token[1] == '>')
					{
						is_append = 1;
					}
					file_after_r = 1;
					is_out_r = 1;
					token = strtok(NULL, DELIMS);
					continue; //skip out redirect token
				}
			case ';':
				{
					command_after_semi = 1;
					token = strtok(NULL, DELIMS);
					continue; //skip semicolon token
				}
			default:
				{
					//first token is always a command
					if ( i == 0 || cmd_after_pipe)
					{
						printf("Command: %s\n", token);
						cmd_after_pipe = 0;
						i++;
					}
					//recognize the file after a redirect
					else if ( file_after_r)
					{
						printf("Redirect file: %s\n", token);

						//if input redirect, open input file it exists
						if(is_in_r)
						{
							if((cmds[num_of_commands - 1].fdIn = open(token, O_RDONLY)) == -1)
							{
								perror("Input file failure: ");
									exit(EXIT_FAILURE);
							}
							is_in_r=0;
						}

						//if output redirect
						else if(is_out_r)
						{
							//if we are appending to the file, open as appension
							if(is_append)
							{
								printf("Appending\n");
								//if it doesn't exists, create it and append
								if ((cmds[num_of_commands - 1].fdOut = open(token, O_WRONLY | O_APPEND, 0744)) == -1)
								{
									if((cmds[num_of_commands - 1].fdOut = open(token, O_WRONLY | O_CREAT | O_APPEND, 0744)) == -1)
									{
										perror(token);
									}
								}
								//else the file exists, apply only append
								else
								{
									closeFd(cmds[num_of_commands - 1].fdOut);
									if((cmds[num_of_commands - 1].fdOut = open(token, O_WRONLY | O_APPEND, 0744)) == -1)
									{
										perror(token);
									}
								}
							}

							//if we are not appending just create and truncate
							else
							{
								if((cmds[num_of_commands - 1].fdOut = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0744)) == -1)
								{
									perror(token);
								}
							}
							is_out_r = 0;
						}

						file_after_r = 0;

					}

					//reconginze the command after a ;
					else if ( command_after_semi)
					{
						printf("Redirection file: %s\n", token);
						command_after_semi = 0;
					}

					//recognize an arg in a cmd
					else
					{
						printf("Arg: %s\n", token);
					}
				}
		}

		//making sure the token ends with a null character and not a new line character
		if(token[strlen(token) - 1] == '\n')
		{
			printf("FOUND NEW LINE- DELETING\n");
			token[strlen(token) - 1] = '\0';
		}
		//checking again
		else if (token[strlen(token)] == '\n')
		{
			printf("FOUND NEW LINE - DELETING\n");
			token[strlen(token)] = '\0';
		}
		token[strlen(token)] = '\0';

		//add the token to the temp token for arguments
		tokenArg[num_tokens] = token;
		num_tokens++;

		token = strtok(NULL, DELIMS);

		}


	//alocate space for args
	for ( i = 0; i < num_of_commands; i++)
	{
		cmds[i].argv = malloc(sizeof(char) * BUFSIZ);
	}


	//build the commands for the args and add them
	i = 0;
	int  argvInd = 0;
	for(j = 0; j < num_tokens; j++)
	{
		if(tokenArg[j][0] == '|')
		{
			argvInd = 0;
			i++;
		}
		else
		{
			cmds[i].argv[argvInd] = tokenArg[j];
			argvInd++;
		}
	}

	return num_of_commands;
}

void run_pipe(Command *commands, int num_cmds, int num_pipes)
{
   int in = STDIN_FILENO;
   int status;
   int i = 0;

      //fork for the number of pipes
   for(i; i < num_pipes; i++){
         int fds[2];
         pid_t child;

         if(pipe(fds) == -1){
            perror("Pipes: ");
            exit(EXIT_FAILURE);
         }

         

         if((child = fork()) == -1 ){
            perror("Fork: ");
            exit(EXIT_FAILURE);
         }

         if(child == 0){
            //child
            closeFd(fds[0]);
            redirect(in, STDIN_FILENO);
            redirect(fds[1], STDOUT_FILENO);

            //child executing
            execvp(commands[i].argv[0], (char * const *) commands[i].argv);
         }

         else //parent
         {
            closeFd(fds[1]);
            closeFd(in);
            in = fds[0];
         }
   }

   //The children have finished. Now the parent must execute the last stage
   //checking io redirection
   int j = 0;
   for(j; j < num_cmds; j++)
   {
      if (commands[j].fdIn && commands[j].fdIn != STDIN_FILENO){
         redirect(in, commands[j].fdIn);
         redirect(STDOUT_FILENO, STDOUT_FILENO);
         execvp(commands[i].argv[0], commands[i].argv);
      }
      else if (commands[j].fdOut && commands[j].fdOut != STDOUT_FILENO){
         redirect(in, STDIN_FILENO);
         redirect(commands[j].fdOut, STDOUT_FILENO);
         execvp(commands[i].argv[0], commands[i].argv);
      }

   }

   redirect(in, STDIN_FILENO);
   redirect(STDOUT_FILENO, STDOUT_FILENO);
   execvp(commands[i].argv[0], (char * const *) commands[i].argv);
}


void closeFd(int fd){
   if(close(fd) == -1 )
   {
      perror(fd);
      exit(EXIT_FAILURE);
   }
}

void redirect(int oldfd, int newfd){

   if(oldfd != newfd){
      if(dup2(oldfd, newfd) == -1){
         perror("dup2: ");
         exit(EXIT_FAILURE);
      }
      else{
         if(close(oldfd) == -1){
            perror(oldfd);
            exit(EXIT_FAILURE);
         }
      }
   }
}

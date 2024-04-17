#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_LINE 1024

int main(int argc, char *argv[]) {
    if (argc != 6) {
        printf("Usage: %s -i <inputdir> -a <outputdir> -t <timelimit> <target src>\n", argv[0]);
        return 1;
    }

    char *inputdir = argv[2];
    char *outputdir = argv[4];
    int timelimit = atoi(argv[5]);
    char *target_src = argv[6];

    glob_t glob;
    struct stat st;

    // 입력 디렉토리 내의 모든 입력 파일 목록화
    glob(inputdir, GLOB_NOSORT, NULL, &glob);

    // 각 입력 파일마다 채점 수행
    for (int i = 0; i < glob.gl_pathc; i++) {
        char *input_file = glob.gl_pathv[i];

        // 입력 파일 정보 확인
        if (stat(input_file, &st) < 0) {
            perror("stat");
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            printf("%s: not a regular file\n", input_file);
            continue;
        }

        // 출력 파일 경로 생성
        char output_file[MAX_LINE];
        snprintf(output_file, sizeof(output_file), "%s/%s.out", outputdir, basename(input_file));

        // 자식 프로세스 생성
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        // 자식 프로세스인 경우
        if (pid == 0) {
            // 입력 파일을 표준 입력으로 연결
            int input_fd = open(input_file, O_RDONLY);
            if (input_fd < 0) {
                perror("open");
                exit(1);
            }

            if (dup2(input_fd, STDIN_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(input_fd);

            // 출력 파일을 표준 출력으로 연결
            int output_fd = open(output_file, O_WRCREAT | O_TRUNC, 0644);
            if (output_fd < 0) {
                perror("open");
                exit(1);
            }

            if (dup2(output_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(output_fd);

            // 시간 제한 설정
            struct rlimit rl;
            rl.rlim_cur = timelimit;
            rl.rlim_max = timelimit;
            if (setrlimit(RLIMIT_REALTIME, &rl) < 0) {
                perror("setrlimit");
                exit(1);
            }

            // 채점 기준 코드 실행
            execve(target_src, NULL, NULL);
            perror("execve");
            exit(1);
        }

        // 부모 프로세스인 경우
        int status;
        waitpid(pid, &status, 0);

        // 자식 프로세스 종료 상태 확인
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                printf("%s: 테스트 케이스 실패\n", input_file);
            }
        } else if (WIFSIGNALED(status)) {
            printf("%s: 시간 제한 초과\n", input_file);
        } else {
            printf("%s: 알 수 없는 오류\n", input_file);
        }
    }

    globfree(&glob);

    return 0;
}
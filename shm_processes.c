#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

/* Two shared integers, inherited by the child after fork(). */
typedef struct {
    volatile int BankAccount;
    volatile int Turn;
} SharedData;

static void ParentProcess(SharedData *shared)
{
    int i;
    for (i = 0; i < 25; ++i) {
        int account, balance;
        sleep(rand() % 6);
        while (shared->Turn != 0) { /* strict alternation: busy wait */ }
        /* Read inside the critical section to avoid a stale balance. */
        account = shared->BankAccount;
        if (account <= 100) {
            balance = rand() % 101;
            if (balance % 2 == 0) {
                account += balance;
                printf("Dear old Dad: Deposits $%d / Balance = $%d\n", balance, account);
            } else {
                printf("Dear old Dad: Doesn't have any money to give\n");
            }
        } else {
            printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", account);
        }
        shared->BankAccount = account;
        shared->Turn = 1;
    }
}

static void ClientProcess(SharedData *shared)
{
    int i;
    for (i = 0; i < 25; ++i) {
        int account, balance;
        sleep(rand() % 6);
        while (shared->Turn != 1) { /* strict alternation: busy wait */ }
        account = shared->BankAccount;
        balance = rand() % 51;
        printf("Poor Student needs $%d\n", balance);
        if (balance <= account) {
            account -= balance;
            printf("Poor Student: Withdraws $%d / Balance = $%d\n", balance, account);
        } else {
            printf("Poor Student: Not Enough Cash ($%d)\n", account);
        }
        shared->BankAccount = account;
        shared->Turn = 0;
    }
}

int main(void)
{
    int ShmID, status;
    pid_t pid, waited;
    SharedData *shared;

    /* Keep output in execution order even when redirected to a file. */
    setvbuf(stdout, NULL, _IONBF, 0);
    ShmID = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0600);
    if (ShmID == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }
    shared = shmat(ShmID, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat");
        shmctl(ShmID, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }
    shared->BankAccount = 0;
    shared->Turn = 0;
    pid = fork();
    if (pid == -1) {
        perror("fork");
        shmdt(shared);
        shmctl(ShmID, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }
    /* Seed after fork so the two processes have different random streams. */
    srand((unsigned int) time(NULL) ^ (unsigned int) getpid());
    if (pid == 0) {
        ClientProcess(shared);
        if (shmdt(shared) == -1) {
            perror("shmdt (child)");
            _exit(EXIT_FAILURE);
        }
        _exit(EXIT_SUCCESS);
    }
    ParentProcess(shared);
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) perror("waitpid");
    if (shmdt(shared) == -1) {
        perror("shmdt (parent)");
        shmctl(ShmID, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }
    if (shmctl(ShmID, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return EXIT_FAILURE;
    }
    return waited != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}

	#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
 
/* ---------------- Policy constants (CO1/CO2) ---------------- */
#define LOAN_PERIOD_DAYS   14     /* initial loan duration            */
#define RENEWAL_DAYS       14     /* extension per renewal            */
#define MAX_RENEWALS        2     /* renewals allowed per loan        */
#define FINE_PER_DAY      2.0     /* Rs. per overdue day              */
#define MAX_FINE_PER_LOAN 500.0   /* fine cap per loan                */
#define INITIAL_CAPACITY    4
#define DATA_FILE "loans.txt"
 
/* ---------------- Structures ---------------- */
typedef struct {
    int day, month, year;         /* day == 0 means "not set"         */
} Date;
 
typedef struct {
    int    book_id;
    int    student_id;
    Date   issue_date;
    Date   due_date;
    Date   return_date;           /* day == 0 -> not yet returned     */
    int    renewals;
    double fine;                  /* final fine (set when returned)   */
} Loan;
 
typedef struct {
    Loan *data;                   /* dynamic array of loans           */
    int   count;
    int   capacity;
} LoanList;
 
static Date today;                /* "current date" used by the system */
 
/* ================= Date utilities ================= */
 
/* Days since 1970-01-01 (civil calendar algorithm) */
long to_days(Date d)
{
    int y = d.year, m = d.month;
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
 
Date from_days(long z)
{
    Date d;
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y   = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp  = (5 * doy + 2) / 153;
    d.day    = (int)(doy - (153 * mp + 2) / 5 + 1);
    d.month  = (int)(mp < 10 ? mp + 3 : mp - 9);
    d.year   = (int)(y + (d.month <= 2));
    return d;
}
 
int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
 
int valid_date(Date d)
{
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (d.year < 1970 || d.year > 2200 || d.month < 1 || d.month > 12) return 0;
    int max = dm[d.month - 1] + (d.month == 2 && is_leap(d.year));
    return d.day >= 1 && d.day <= max;
}
 
Date add_days(Date d, int n) { return from_days(to_days(d) + n); }
 
int is_returned(const Loan *l) { return l->return_date.day != 0; }
 
void print_date(Date d)
{
    if (d.day == 0) printf("%-10s", "--");
    else            printf("%02d/%02d/%04d", d.day, d.month, d.year);
}
 
/* ================= Input helpers ================= */
 
int read_int(const char *prompt)
{
    char buf[64];
    long v;
    char *end;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof buf, stdin)) exit(0);
        v = strtol(buf, &end, 10);
        if (end != buf && (*end == '\n' || *end == '\0') && v >= 0 && v < 100000000)
            return (int)v;
        printf("  Invalid number, try again.\n");
    }
}
 
Date read_date(const char *prompt)
{
    Date d;
    char buf[64];
    for (;;) {
        printf("%s (DD MM YYYY): ", prompt);
        if (!fgets(buf, sizeof buf, stdin)) exit(0);
        if (sscanf(buf, "%d %d %d", &d.day, &d.month, &d.year) == 3 && valid_date(d))
            return d;
        printf("  Invalid date, try again.\n");
    }
}
 
/* ================= Dynamic memory (CO4) ================= */
 
void list_init(LoanList *L)
{
    L->data = (Loan *)malloc(INITIAL_CAPACITY * sizeof(Loan));
    if (!L->data) { fprintf(stderr, "Out of memory\n"); exit(1); }
    L->count = 0;
    L->capacity = INITIAL_CAPACITY;
}
 
void list_free(LoanList *L)
{
    free(L->data);
    L->data = NULL;
    L->count = L->capacity = 0;
}
 
/* Returns pointer to the new slot, growing the array when full */
Loan *list_add(LoanList *L)
{
    if (L->count == L->capacity) {
        int newcap = L->capacity * 2;
        Loan *tmp = (Loan *)realloc(L->data, newcap * sizeof(Loan));
        if (!tmp) { fprintf(stderr, "Out of memory\n"); return NULL; }
        L->data = tmp;
        L->capacity = newcap;
    }
    return &L->data[L->count++];
}
 
/* ================= Fine calculation (CO1, CO2) ================= */
 
/* Overdue days as of 'as_of' (or return date if returned) */
long overdue_days(const Loan *l, Date as_of)
{
    Date end = is_returned(l) ? l->return_date : as_of;
    long diff = to_days(end) - to_days(l->due_date);
    return diff > 0 ? diff : 0;
}
 
double calc_fine(const Loan *l, Date as_of)
{
    double f = overdue_days(l, as_of) * FINE_PER_DAY;
    return f > MAX_FINE_PER_LOAN ? MAX_FINE_PER_LOAN : f;
}
 
/* ================= Core operations ================= */
 
Loan *find_active_loan(LoanList *L, int book_id)
{
    for (Loan *p = L->data; p < L->data + L->count; p++)
        if (p->book_id == book_id && !is_returned(p))
            return p;
    return NULL;
}
 
void issue_book(LoanList *L)
{
    printf("\n--- Issue Book ---\n");
    int book = read_int("Book ID    : ");
    if (find_active_loan(L, book)) {
        printf("Book %d is already issued to someone.\n", book);
        return;
    }
    int student = read_int("Student ID : ");
    Loan *n = list_add(L);
    if (!n) return;
    n->book_id    = book;
    n->student_id = student;
    n->issue_date = today;
    n->due_date   = add_days(today, LOAN_PERIOD_DAYS);
    n->return_date.day = n->return_date.month = n->return_date.year = 0;
    n->renewals   = 0;
    n->fine       = 0.0;
    printf("Issued on ");  print_date(n->issue_date);
    printf(". Due on ");   print_date(n->due_date);
    printf(".\n");
}
 
/* Renewal decision logic (CO2) */
void renew_book(LoanList *L)
{
    printf("\n--- Renew Book ---\n");
    int book = read_int("Book ID: ");
    Loan *l = find_active_loan(L, book);
    if (!l) { printf("No active loan for book %d.\n", book); return; }
 
    if (to_days(today) > to_days(l->due_date)) {
        printf("Renewal DENIED: book is overdue (%ld day(s)). Return it and pay fine Rs. %.2f.\n",
               overdue_days(l, today), calc_fine(l, today));
    } else if (l->renewals >= MAX_RENEWALS) {
        printf("Renewal DENIED: maximum of %d renewals reached.\n", MAX_RENEWALS);
    } else {
        l->due_date = add_days(l->due_date, RENEWAL_DAYS);
        l->renewals++;
        printf("Renewal APPROVED (%d/%d). New due date: ", l->renewals, MAX_RENEWALS);
        print_date(l->due_date);
        printf("\n");
    }
}
 
void return_book(LoanList *L)
{
    printf("\n--- Return Book ---\n");
    int book = read_int("Book ID: ");
    Loan *l = find_active_loan(L, book);
    if (!l) { printf("No active loan for book %d.\n", book); return; }
 
    Date r = read_date("Return date");
    if (to_days(r) < to_days(l->issue_date)) {
        printf("Return date cannot be before issue date.\n");
        return;
    }
    l->return_date = r;
    l->fine = calc_fine(l, r);
    printf("Book %d returned by student %d.\n", l->book_id, l->student_id);
    if (l->fine > 0)
        printf("Overdue by %ld day(s). FINE COLLECTED: Rs. %.2f\n", overdue_days(l, r), l->fine);
    else
        printf("Returned on time. No fine.\n");
}
 
/* ================= Reports (CO3) ================= */
 
void print_header(void)
{
    printf("%-6s %-8s %-11s %-11s %-11s %-4s %-9s %-10s\n",
           "Book", "Student", "Issued", "Due", "Returned", "Ren", "Fine(Rs)", "Status");
    printf("----------------------------------------------------------------------------\n");
}
 
void print_loan(const Loan *l)
{
    double fine = is_returned(l) ? l->fine : calc_fine(l, today);
    const char *status = is_returned(l) ? "Returned"
                       : (to_days(today) > to_days(l->due_date) ? "OVERDUE" : "Active");
    printf("%-6d %-8d ", l->book_id, l->student_id);
    print_date(l->issue_date);  printf(" ");
    print_date(l->due_date);    printf(" ");
    print_date(l->return_date); printf(" ");
    printf("%-4d %-9.2f %-10s\n", l->renewals, fine, status);
}
 
void show_all(const LoanList *L)
{
    printf("\n--- All Loans (as of "); print_date(today); printf(") ---\n");
    if (L->count == 0) { printf("No loans recorded.\n"); return; }
    print_header();
    for (int i = 0; i < L->count; i++) print_loan(&L->data[i]);
}
 
void show_student(const LoanList *L)
{
    int sid = read_int("Student ID: ");
    int found = 0;
    for (int i = 0; i < L->count; i++) {
        if (L->data[i].student_id == sid) {
            if (!found) { printf("\n"); print_header(); }
            print_loan(&L->data[i]);
            found = 1;
        }
    }
    if (!found) printf("No loans found for student %d.\n", sid);
}
 
/* qsort comparator: highest fine first (works on array of Loan pointers) */
static int cmp_fine_desc(const void *a, const void *b)
{
    double fa = calc_fine(*(Loan *const *)a, today);
    double fb = calc_fine(*(Loan *const *)b, today);
    return (fa < fb) - (fa > fb);
}
 
void defaulter_report(const LoanList *L)
{
    printf("\n=========== DEFAULTER REPORT (as of "); print_date(today); printf(") ===========\n");
 
    /* Temporary array of pointers to overdue, unreturned loans */
    Loan **def = (Loan **)malloc((L->count ? L->count : 1) * sizeof(Loan *));
    if (!def) { fprintf(stderr, "Out of memory\n"); return; }
    int n = 0;
    for (int i = 0; i < L->count; i++)
        if (!is_returned(&L->data[i]) && to_days(today) > to_days(L->data[i].due_date))
            def[n++] = &L->data[i];
 
    if (n == 0) {
        printf("No defaulters. All issued books are within due date.\n");
        free(def);
        return;
    }
    qsort(def, n, sizeof(Loan *), cmp_fine_desc);
 
    printf("%-6s %-8s %-11s %-9s %-10s\n", "Book", "Student", "Due", "DaysLate", "Fine(Rs)");
    printf("------------------------------------------------\n");
    double total = 0;
    for (int i = 0; i < n; i++) {
        double f = calc_fine(def[i], today);
        printf("%-6d %-8d ", def[i]->book_id, def[i]->student_id);
        print_date(def[i]->due_date);
        printf(" %-9ld %-10.2f\n", overdue_days(def[i], today), f);
        total += f;
    }
    printf("------------------------------------------------\n");
    printf("Defaulting loans: %d    Total pending fines: Rs. %.2f\n", n, total);
    free(def);
}
 
void revenue_report(const LoanList *L)
{
    int returned = 0, fined = 0, active = 0, overdue = 0;
    double collected = 0, pending = 0;
 
    for (int i = 0; i < L->count; i++) {
        const Loan *l = &L->data[i];
        if (is_returned(l)) {
            returned++;
            if (l->fine > 0) { fined++; collected += l->fine; }
        } else {
            active++;
            double f = calc_fine(l, today);
            if (f > 0) { overdue++; pending += f; }
        }
    }
    printf("\n=========== REVENUE REPORT (as of "); print_date(today); printf(") ===========\n");
    printf("Total loans recorded      : %d\n", L->count);
    printf("Returned loans            : %d (with fines: %d)\n", returned, fined);
    printf("Active loans              : %d (overdue: %d)\n", active, overdue);
    printf("Fine revenue collected    : Rs. %.2f\n", collected);
    printf("Fines pending (unreturned): Rs. %.2f\n", pending);
    printf("Projected total revenue   : Rs. %.2f\n", collected + pending);
    if (returned > 0)
        printf("Avg fine per returned loan: Rs. %.2f\n", collected / returned);
}
 
/* ================= File handling ================= */
 
int save_loans(const LoanList *L)
{
    FILE *fp = fopen(DATA_FILE, "w");
    if (!fp) { printf("Error: cannot write %s\n", DATA_FILE); return 0; }
    for (int i = 0; i < L->count; i++) {
        const Loan *l = &L->data[i];
        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %.2f\n",
                l->book_id, l->student_id,
                l->issue_date.day,  l->issue_date.month,  l->issue_date.year,
                l->due_date.day,    l->due_date.month,    l->due_date.year,
                l->return_date.day, l->return_date.month, l->return_date.year,
                l->renewals, 0, l->fine);
    }
    fclose(fp);
    return 1;
}
 
void load_loans(LoanList *L)
{
    FILE *fp = fopen(DATA_FILE, "r");
    if (!fp) return;                       /* first run: no file yet */
    Loan t;
    int reserved;
    while (fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %lf",
                  &t.book_id, &t.student_id,
                  &t.issue_date.day,  &t.issue_date.month,  &t.issue_date.year,
                  &t.due_date.day,    &t.due_date.month,    &t.due_date.year,
                  &t.return_date.day, &t.return_date.month, &t.return_date.year,
                  &t.renewals, &reserved, &t.fine) == 14) {
        Loan *n = list_add(L);
        if (!n) break;
        *n = t;
    }
    fclose(fp);
    printf("Loaded %d loan record(s) from %s.\n", L->count, DATA_FILE);
}
 
/* ================= Main menu ================= */
 
void print_menu(void)
{
    printf("\n=========== LIBRARY FINE & RENEWAL SYSTEM ===========\n");
    printf("System date: "); print_date(today); printf("\n");
    printf(" 1. Issue book\n");
    printf(" 2. Renew book\n");
    printf(" 3. Return book (calculate fine)\n");
    printf(" 4. View all loans\n");
    printf(" 5. View loans of a student\n");
    printf(" 6. Defaulter report\n");
    printf(" 7. Revenue report\n");
    printf(" 8. Change system date (for simulation)\n");
    printf(" 9. Save\n");
    printf(" 0. Save and exit\n");
    printf("Policy: %d-day loan, %d renewals of %d days, Rs. %.2f/day fine (cap Rs. %.0f)\n",
           LOAN_PERIOD_DAYS, MAX_RENEWALS, RENEWAL_DAYS, FINE_PER_DAY, MAX_FINE_PER_LOAN);
}
 
int main(void)
{
    LoanList loans;
    list_init(&loans);
 
    /* Default system date = real current date */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    today.day = tm->tm_mday; today.month = tm->tm_mon + 1; today.year = tm->tm_year + 1900;
 
    load_loans(&loans);
 
    int choice;
    do {
        print_menu();
        choice = read_int("Enter choice: ");
        switch (choice) {
            case 1: issue_book(&loans);       break;
            case 2: renew_book(&loans);       break;
            case 3: return_book(&loans);      break;
            case 4: show_all(&loans);         break;
            case 5: show_student(&loans);     break;
            case 6: defaulter_report(&loans); break;
            case 7: revenue_report(&loans);   break;
            case 8: today = read_date("New system date"); break;
            case 9: if (save_loans(&loans)) printf("Saved to %s.\n", DATA_FILE); break;
            case 0: if (save_loans(&loans)) printf("Data saved. Goodbye!\n"); break;
            default: printf("Invalid choice.\n");
        }
    } while (choice != 0);
 
    list_free(&loans);
    return 0;
}

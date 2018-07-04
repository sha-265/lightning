#include "config.h"
#include <signal.h>
#include <setjmp.h>
#include <lightningd/jsonrpc.h>

#include <lightningd/params.c>
#include <common/json.c>
#include <common/json_escaped.c>
#include <ccan/array_size/array_size.h>
#include <ccan/err/err.h>
#include <unistd.h>

bool failed;
char *fail_msg;

struct command *cmd;

void command_fail(struct command *cmd, int code, const char *fmt, ...)
{
	failed = true;
	va_list ap;
	va_start(ap, fmt);
	fail_msg = tal_vfmt(cmd, fmt, ap);
	va_end(ap);
}

void command_fail_detailed(struct command *cmd, int code,
			   const struct json_result *data, const char *fmt, ...)
{
	failed = true;
	va_list ap;
	va_start(ap, fmt);
	fail_msg = tal_vfmt(cmd, fmt, ap);
	fail_msg =
	    tal_fmt(cmd, "%s data: %s", fail_msg, json_result_string(data));
	va_end(ap);
}

/* AUTOGENERATED MOCKS START */
/* Generated stub for json_tok_wtx */
bool json_tok_wtx(struct wallet_tx *tx UNNEEDED, const char *buffer UNNEEDED,
		  const jsmntok_t * sattok UNNEEDED)
{
	abort();
}

/* AUTOGENERATED MOCKS END */

struct json {
	jsmntok_t *toks;
	char *buffer;
};

static void convert_quotes(char *first)
{
	while (*first != '\0') {
		if (*first == '\'')
			*first = '"';
		first++;
	}
}

static struct json *json_parse(const tal_t * ctx, const char *str)
{
	struct json *j = tal(ctx, struct json);
	j->buffer = tal_strdup(j, str);
	convert_quotes(j->buffer);

	j->toks = tal_arr(j, jsmntok_t, 50);
	assert(j->toks);
	jsmn_parser parser;

      again:
	jsmn_init(&parser);
	int ret = jsmn_parse(&parser, j->buffer, strlen(j->buffer), j->toks,
			     tal_count(j->toks));
	if (ret == JSMN_ERROR_NOMEM) {
		tal_resize(&j->toks, tal_count(j->toks) * 2);
		goto again;
	}

	if (ret <= 0) {
		assert(0);
	}
	failed = false;
	return j;
}

static void zero_params(void)
{
	struct json *j = json_parse(cmd, "{}");
	assert(param_parse(cmd, j->buffer, j->toks, NULL));

	j = json_parse(cmd, "[]");
	assert(param_parse(cmd, j->buffer, j->toks, NULL));
}

struct sanity {
	char *str;
	bool failed;
	int ival;
	double dval;
	char *fail_str;
};

struct sanity buffers[] = {
	// pass
	{"['42', '3.15']", false, 42, 3.15, NULL},
	{"{ 'u64' : '42', 'double' : '3.15' }", false, 42, 3.15, NULL},

	// fail
	{"{'u64':'42', 'double':'3.15', 'extra':'stuff'}", true, 0, 0,
	 "unknown parameter"},
	{"['42', '3.15', 'stuff']", true, 0, 0, "too many"},
	{"['42', '3.15', 'null']", true, 0, 0, "too many"},

	// not enough
	{"{'u64':'42'}", true, 0, 0, "missing required"},
	{"['42']", true, 0, 0, "missing required"},

	// fail wrong type
	{"{'u64':'hello', 'double':'3.15'}", true, 0, 0, "\"u64\": \"hello\""},
	{"['3.15', '3.15', 'stuff']", true, 0, 0, "integer"},
};

static void stest(const struct json *j, struct sanity *b)
{
	u64 ival;
	double dval;
	if (!param_parse(cmd, j->buffer, j->toks,
			 param_req("u64", json_tok_u64, &ival),
			 param_req("double", json_tok_double, &dval), NULL)) {
		assert(failed == true);
		assert(b->failed == true);
		assert(strstr(fail_msg, b->fail_str));
	} else {
		assert(b->failed == false);
		assert(ival == 42);
		assert(dval > 3.1499 && b->dval < 3.1501);
	}
}

static void sanity(void)
{
	for (int i = 0; i < ARRAY_SIZE(buffers); ++i) {
		struct json *j = json_parse(cmd, buffers[i].str);
		assert(j->toks->type == JSMN_OBJECT
		       || j->toks->type == JSMN_ARRAY);
		stest(j, &buffers[i]);
	}
}

/*
 * Make sure toks are passed through correctly, and also make sure
 * optional missing toks are set to NULL.
 */
static void tok_tok(void)
{
	{
		unsigned int n;
		const jsmntok_t *tok = NULL;
		struct json *j = json_parse(cmd, "{ 'satoshi', '546' }");

		assert(param_parse(cmd, j->buffer, j->toks,
				   param_req("satoshi", json_tok_tok,
					     &tok), NULL));
		assert(tok);
		assert(json_tok_number(j->buffer, tok, &n));
		assert(n == 546);
	}
	// again with missing optional parameter
	{
		/* make sure it is *not* NULL */
		const jsmntok_t *tok = (const jsmntok_t *) 65535;

		struct json *j = json_parse(cmd, "{}");
		assert(param_parse(cmd, j->buffer, j->toks,
				   param_opt("satoshi", json_tok_tok, &tok), NULL));

		/* make sure it *is* NULL */
		assert(tok == NULL);
	}
}

/* check for valid but duplicate json name-value pairs */
static void dup_names(void)
{
	struct json *j =
	    json_parse(cmd,
		       "{ 'u64' : '42', 'u64' : '43', 'double' : '3.15' }");

	u64 i;
	double d;
	assert(!param_parse(cmd, j->buffer, j->toks,
			    param_req("u64", json_tok_u64, &i),
			    param_req("double", json_tok_double, &d), NULL));
}

static void null_params(void)
{
	uint64_t *ints = tal_arr(cmd, uint64_t, 4);
	uint64_t **intptrs = tal_arr(cmd, uint64_t *, 3);
	/* no null params */
	struct json *j =
	    json_parse(cmd, "[ '10', '11', '12', '13', '14', '15', '16']");
	for (int i = 0; i < tal_count(ints); ++i)
		ints[i] = i;

	assert(param_parse(cmd, j->buffer, j->toks,
			   param_req("0", json_tok_u64, &ints[0]),
			   param_req("1", json_tok_u64, &ints[1]),
			   param_req("2", json_tok_u64, &ints[2]),
			   param_req("3", json_tok_u64, &ints[3]),
			   param_opt("4", json_tok_u64, &intptrs[0]),
			   param_opt("5", json_tok_u64, &intptrs[1]),
			   param_opt("6", json_tok_u64, &intptrs[2]),
			   NULL));
	for (int i = 0; i < tal_count(ints); ++i)
		assert(ints[i] == i + 10);
	for (int i = 0; i < tal_count(intptrs); ++i)
		assert(*intptrs[i] == i + 10 + tal_count(ints));

	/* missing at end */
	for (int i = 0; i < tal_count(ints); ++i)
		ints[i] = 42;
	for (int i = 0; i < tal_count(intptrs); ++i)
		intptrs[i] = (void *)42;

	j = json_parse(cmd, "[ '10', '11', '12', '13', '14']");
	assert(param_parse(cmd, j->buffer, j->toks,
			   param_req("0", json_tok_u64, &ints[0]),
			   param_req("1", json_tok_u64, &ints[1]),
			   param_req("2", json_tok_u64, &ints[2]),
			   param_req("3", json_tok_u64, &ints[3]),
			   param_opt("4", json_tok_u64, &intptrs[0]),
			   param_opt("5", json_tok_u64, &intptrs[1]),
			   param_opt("6", json_tok_u64, &intptrs[2]),
			   NULL));
	assert(*intptrs[0] == 14);
	assert(intptrs[1] == NULL);
	assert(intptrs[2] == NULL);
}

#if DEVELOPER
jmp_buf jump;
static void handle_abort(int sig)
{
	longjmp(jump, 1);
}

static int set_assert(void)
{
	struct sigaction act;
	int old_stderr;
	memset(&act, '\0', sizeof(act));
	act.sa_handler = &handle_abort;
	if (sigaction(SIGABRT, &act, NULL) < 0)
		err(1, "set_assert");

	/* Don't spam with assert messages. */
	old_stderr = dup(STDERR_FILENO);
	close(STDERR_FILENO);
	return old_stderr;
}

static void restore_assert(int old_stderr)
{
	struct sigaction act;

	dup2(old_stderr, STDERR_FILENO);
	close(old_stderr);
	memset(&act, '\0', sizeof(act));
	act.sa_handler = SIG_DFL;
	if (sigaction(SIGABRT, &act, NULL) < 0)
		err(1, "restore_assert");

}

/*
 * Check to make sure there are no programming mistakes.
 */
static void bad_programmer(void)
{
	u64 ival;
	u64 ival2;
	double dval;
	struct json *j = json_parse(cmd, "[ '25', '546', '26' ]");
	int old_stderr = set_assert();

	/* check for repeated names */
	if (setjmp(jump) == 0) {
		param_parse(cmd, j->buffer, j->toks,
			    param_req("repeat", json_tok_u64, &ival),
			    param_req("double", json_tok_double, &dval),
			    param_req("repeat", json_tok_u64, &ival2), NULL);
		/* shouldn't get here */
		restore_assert(old_stderr);
		assert(false);
	}

	if (setjmp(jump) == 0) {
		param_parse(cmd, j->buffer, j->toks,
			    param_req("repeat", json_tok_u64, &ival),
			    param_req("double", json_tok_double, &dval),
			    param_req("repeat", json_tok_u64, &ival), NULL);
		restore_assert(old_stderr);
		assert(false);
	}

	if (setjmp(jump) == 0) {
		param_parse(cmd, j->buffer, j->toks,
			    param_req("u64", json_tok_u64, &ival),
			    param_req("repeat", json_tok_double, &dval),
			    param_req("repeat", json_tok_double, &dval), NULL);
		restore_assert(old_stderr);
		assert(false);
	}

	/* check for repeated arguments */
	if (setjmp(jump) == 0) {
		param_parse(cmd, j->buffer, j->toks,
			    param_req("u64", json_tok_u64, &ival),
			    param_req("repeated-arg", json_tok_u64, &ival),
			    NULL);
		restore_assert(old_stderr);
		assert(false);
	}

	if (setjmp(jump) == 0) {
		param_parse(cmd, j->buffer, j->toks,
			    param_req("u64", NULL, &ival), NULL);
		restore_assert(old_stderr);
		assert(false);
	}

	if (setjmp(jump) == 0) {
		/* Add required param after optional */
		struct json *j =
		    json_parse(cmd, "[ '25', '546', '26', '1.1' ]");
		unsigned int *msatoshi;
		double riskfactor;
		param_parse(cmd, j->buffer, j->toks,
			    param_req("u64", json_tok_u64, &ival),
			    param_req("double", json_tok_double, &dval),
			    param_opt("msatoshi",
				      json_tok_number, &msatoshi),
			    param_req("riskfactor", json_tok_double,
				      &riskfactor), NULL);
		restore_assert(old_stderr);
		assert(false);
	}
	restore_assert(old_stderr);
}
#endif

static void add_members(struct param **params,
			struct json_result *obj,
			struct json_result *arr, unsigned int *ints)
{
	for (int i = 0; i < tal_count(ints); ++i) {
		char *name = tal_fmt(tmpctx, "%i", i);
		json_add_num(obj, name, i);
		json_add_num(arr, NULL, i);
		param_add(params, name,
			  typesafe_cb_preargs(bool, void *,
					      json_tok_number,
					      &ints[i],
					      const char *,
					      const jsmntok_t *),
			  &ints[i], 0);
	}
}

/*
 * A roundabout way of initializing an array of ints to:
 * ints[0] = 0, ints[1] = 1, ... ints[499] = 499
 */
static void five_hundred_params(void)
{
	struct param *params = tal_arr(NULL, struct param, 0);

	unsigned int *ints = tal_arr(params, unsigned int, 500);
	struct json_result *obj = new_json_result(params);
	struct json_result *arr = new_json_result(params);
	json_object_start(obj, NULL);
	json_array_start(arr, NULL);
	add_members(&params, obj, arr, ints);
	json_object_end(obj);
	json_array_end(arr);

	/* first test object version */
	struct json *j = json_parse(params, obj->s);
	assert(param_parse_arr(cmd, j->buffer, j->toks, params));
	for (int i = 0; i < tal_count(ints); ++i) {
		assert(ints[i] == i);
		ints[i] = 65535;
	}

	/* now test array */
	j = json_parse(params, arr->s);
	assert(param_parse_arr(cmd, j->buffer, j->toks, params));
	for (int i = 0; i < tal_count(ints); ++i) {
		assert(ints[i] == i);
	}

	tal_free(params);
}

static void sendpay(void)
{
	struct json *j = json_parse(cmd, "[ 'A', '123', 'hello there' '547']");

	const jsmntok_t *routetok, *note;
	u64 *msatoshi;
	unsigned cltv;

	if (!param_parse(cmd, j->buffer, j->toks,
			 param_req("route", json_tok_tok, &routetok),
			 param_req("cltv", json_tok_number, &cltv),
			 param_opt("note", json_tok_tok, &note),
			 param_opt("msatoshi", json_tok_u64, &msatoshi),
			 NULL))
		assert(false);

	assert(note);
	assert(!strncmp("hello there", j->buffer + note->start, note->end - note->start));
	assert(msatoshi);
	assert(*msatoshi == 547);
}

static void sendpay_nulltok(void)
{
	struct json *j = json_parse(cmd, "[ 'A', '123']");

	const jsmntok_t *routetok, *note = (void *) 65535;
	u64 *msatoshi;
	unsigned cltv;

	if (!param_parse(cmd, j->buffer, j->toks,
			 param_req("route", json_tok_tok, &routetok),
			 param_req("cltv", json_tok_number, &cltv),
			 param_opt("note", json_tok_tok, &note),
			 param_opt("msatoshi", json_tok_u64, &msatoshi),
			 NULL))
		assert(false);

	assert(note == NULL);
	assert(msatoshi == NULL);
}

int main(void)
{
	setup_locale();
	setup_tmpctx();
	cmd = tal(tmpctx, struct command);
	fail_msg = tal_arr(cmd, char, 10000);

	zero_params();
	sanity();
	tok_tok();
	null_params();
#if DEVELOPER
	bad_programmer();
#endif
	dup_names();
	five_hundred_params();
	sendpay();
	sendpay_nulltok();
	tal_free(tmpctx);
	printf("run-params ok\n");
}

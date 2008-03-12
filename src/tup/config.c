#include "config.h"
#include "slurp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t')
#define IS_TOKEN(c) ((c) == ' ' || (c) == '\t' || (c) == '=' || (c) == '\n')

static void init_default_config(struct tup_config *cfg);
static int get_value_pair(struct buf *file, int *pos,
			  struct buf *lval, struct buf *rval);
static int skip_whitespace(struct buf *file, int *pos);

static char default_build_so[] = "builder.so";

int load_tup_config(struct tup_config *cfg)
{
	int fd;
	int rc = -1;
	struct buf cfg_file;
	struct buf lval;
	struct buf rval;
	int pos = 0;

	init_default_config(cfg);

	fd = open(TUP_CONFIG, O_RDONLY);
	if(fd < 0)
		return 0; /* No config is ok */
	if(fslurp(fd, &cfg_file) < 0)
		goto out;

	while((rc = get_value_pair(&cfg_file, &pos, &lval, &rval)) > 0) {
		if(tup_config_set_param(cfg, &lval, &rval) < 0)
			return -1;
	}
	free(cfg_file.s);

out:
	close(fd);
	return rc;
}

int save_tup_config(const struct tup_config *cfg)
{
	FILE *f;

	f = fopen(TUP_CONFIG, "w+");
	if(!f) {
		perror(TUP_CONFIG);
		return -1;
	}
	if(cfg->build_so != default_build_so)
		fprintf(f, "build_so = %s\n", cfg->build_so);

	fclose(f);
	return 0;
}

void print_tup_config(const struct tup_config *cfg, const char *key)
{
	int color;
	printf("tup config:\n");
	if(key && strcmp(key, "build_so") == 0)
		color = 32;
	else if(cfg->build_so != default_build_so)
		color = 34;
	else
		color = 0;
	printf("[%im  build_so: '%s'[0m\n", color, cfg->build_so);
}

int tup_config_set_param(struct tup_config *cfg, struct buf *lval,
			 struct buf *rval)
{
	int def = 0;
	if(buf_cmp(rval, "default") == 0)
		def = 1;

	if(buf_cmp(lval, "build_so") == 0) {
		if(def) {
			cfg->build_so = default_build_so;
		} else {
			cfg->build_so = buf_to_string(rval);
			if(!cfg->build_so)
				return -1;
		}
	}
	return 0;
}

static void init_default_config(struct tup_config *cfg)
{
	cfg->build_so = default_build_so;
}

static int get_value_pair(struct buf *file, int *pos,
			  struct buf *lval, struct buf *rval)
{
	if(skip_whitespace(file, pos) < 0)
		return 0;

	lval->len = 0;
	lval->s = &file->s[*pos];
	for(; *pos < file->len; (*pos)++) {
		if(IS_TOKEN(file->s[*pos]))
			break;
		lval->len++;
	}
	if(!lval->len) {
		fprintf(stderr, "Parse error at position %i: "
			"missing 'lvalue'\n", *pos);
		return -1;
	}

	if(skip_whitespace(file, pos) < 0) {
		fprintf(stderr, "Parse error at position %i: "
			"missing '= rvalue' (1)\n", *pos);
		return -1;
	}
	if(file->s[*pos] != '=') {
		fprintf(stderr, "Parse error at position %i: "
			"missing '= rvalue' (2)\n", *pos);
		return -1;
	}
	(*pos)++;
	if(skip_whitespace(file, pos) < 0) {
		fprintf(stderr, "Parse error at position %i: "
			"missing 'rvalue'\n", *pos);
		return -1;
	}

	rval->len = 0;
	rval->s = &file->s[*pos];
	for(; *pos < file->len; (*pos)++) {
		if(IS_TOKEN(file->s[*pos]))
			break;
		rval->len++;
	}
	if(rval->len == 0) {
		fprintf(stderr, "Parse error at position %i: "
			"missing 'rvalue'\n", *pos);
		return -1;
	}

	if(skip_whitespace(file, pos) < 0) {
		fprintf(stderr, "Parse error at position %i: "
			"missing extraneous data at end of line\n", *pos);
		return -1;
	}
	if(file->s[*pos] != '\n') {
		fprintf(stderr, "Parse error at position %i: "
			"expecting newline\n", *pos);
		return -1;
	}
	(*pos)++;
	return 1;
}

static int skip_whitespace(struct buf *file, int *pos)
{
	for(; *pos < file->len; (*pos)++) {
		if(!IS_WHITESPACE(file->s[*pos]))
			return 0;
	}
	return -1;
}

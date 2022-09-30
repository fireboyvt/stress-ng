/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 * Copyright (C)      2022 Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-sort.h"

#if defined(HAVE_SEARCH_H)
#include <search.h>
#endif

#define TSEARCH_SIZE_SHIFT	(22)
#define MIN_TSEARCH_SIZE	(1 * KB)
#define MAX_TSEARCH_SIZE	(1U << TSEARCH_SIZE_SHIFT)	/* 4 MB */
#define DEFAULT_TSEARCH_SIZE	(64 * KB)

static const stress_help_t help[] = {
	{ NULL,	"tsearch N",		"start N workers that exercise a tree search" },
	{ NULL,	"tsearch-ops N",	"stop after N tree search bogo operations" },
	{ NULL,	"tsearch-size N",	"number of 32 bit integers to tsearch" },
	{ NULL,	NULL,			NULL }
};

/*
 *  stress_set_tsearch_size()
 *      set tsearch size from given option string
 */
static int stress_set_tsearch_size(const char *opt)
{
	uint64_t tsearch_size;

	tsearch_size = stress_get_uint64(opt);
	stress_check_range("tsearch-size", tsearch_size,
		MIN_TSEARCH_SIZE, MAX_TSEARCH_SIZE);
	return stress_set_setting("tsearch-size", TYPE_ID_UINT64, &tsearch_size);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_tsearch_size,	stress_set_tsearch_size },
	{ 0,			NULL }
};

#if defined(HAVE_TSEARCH)

/*
 *  stress_tsearch()
 *	stress tsearch
 */
static int stress_tsearch(const stress_args_t *args)
{
	uint64_t tsearch_size = DEFAULT_TSEARCH_SIZE;
	int32_t *data;
	size_t i, n;

	if (!stress_get_setting("tsearch-size", &tsearch_size)) {
		if (g_opt_flags & OPT_FLAGS_MAXIMIZE)
			tsearch_size = MAX_TSEARCH_SIZE;
		if (g_opt_flags & OPT_FLAGS_MINIMIZE)
			tsearch_size = MIN_TSEARCH_SIZE;
	}
	n = (size_t)tsearch_size;

	if ((data = calloc(n, sizeof(*data))) == NULL) {
		pr_fail("%s: calloc failed, out of memory\n", args->name);
		return EXIT_NO_RESOURCE;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		void *root = NULL;

		/* Step #1, populate tree */
		for (i = 0; i < n; i++) {
			data[i] = (int32_t)(((stress_mwc16() & 0xfff) << TSEARCH_SIZE_SHIFT) ^ i);
			if (tsearch(&data[i], &root, stress_sort_cmp_int32) == NULL) {
				size_t j;

				pr_err("%s: cannot allocate new "
					"tree node\n", args->name);
				for (j = 0; j < i; j++)
					tdelete(&data[j], &root, stress_sort_cmp_int32);
				goto abort;
			}
		}
		/* Step #2, find */
		for (i = 0; keep_stressing_flag() && i < n; i++) {
			const void **result = tfind(&data[i], &root, stress_sort_cmp_int32);

			if (g_opt_flags & OPT_FLAGS_VERIFY) {
				if (!result) {
					pr_fail("%s: element %zu could not be found\n",
						args->name, i);
				} else {
					const int32_t *val = *result;

					if (*val != data[i]) {
						pr_fail("%s: element "
							"%zu found %" PRIu32
							", expecting %" PRIu32 "\n",
							args->name, i, *val, data[i]);
					}
				}
			}
		}
		/* Step #3, delete */
		for (i = 0; i < n; i++) {
			const void **result = tdelete(&data[i], &root, stress_sort_cmp_int32);

			if ((g_opt_flags & OPT_FLAGS_VERIFY) && (result == NULL)) {
				pr_fail("%s: element %zu could not be found\n",
					args->name, i);
			}
		}
		inc_counter(args);
	} while (keep_stressing(args));

abort:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	free(data);
	return EXIT_SUCCESS;
}

stressor_info_t stress_tsearch_info = {
	.stressor = stress_tsearch,
	.class = CLASS_CPU_CACHE | CLASS_CPU | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_OPTIONAL,
	.help = help
};

#else

stressor_info_t stress_tsearch_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_CPU_CACHE | CLASS_CPU | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_OPTIONAL,
	.help = help
};

#endif

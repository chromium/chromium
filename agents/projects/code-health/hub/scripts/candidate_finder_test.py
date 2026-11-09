# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for candidate_finder.py."""

import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import candidate_finder
import hub_utils

ALPHA_SUBSYSTEM = 'chrome/browser/alpha'
ALPHA_FILE_1 = 'chrome/browser/alpha/A1.java'
ALPHA_FILE_2 = 'chrome/browser/alpha/A2.java'
BETA_SUBSYSTEM = 'components/beta'
BETA_FILE_1 = 'components/beta/B1.java'


def _run_and_capture(fn, *args, **kwargs):
    """Runs |fn| and returns everything it printed to stdout."""
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        fn(*args, **kwargs)
    return out.getvalue()


def _make_grouped_candidates():
    return [
        {'file': ALPHA_FILE_1, 'name': 'alpha1'},
        {'file': ALPHA_FILE_2, 'name': 'alpha2'},
        {'file': BETA_FILE_1, 'name': 'beta1'},
    ]


class HandleGroupedModeTest(unittest.TestCase):
    def _run_json(self, candidates, **kwargs):
        output = _run_and_capture(
            candidate_finder.handle_grouped_mode,
            candidates,
            10,
            json_output=True,
            **kwargs,
        )
        return json.loads(output)

    def _files(self, result):
        return {c['file'] for c in result['candidates']}

    def test_exclude_by_file(self):
        result = self._run_json(
            _make_grouped_candidates(), exclude=[ALPHA_FILE_1]
        )
        self.assertNotIn(ALPHA_FILE_1, self._files(result))

    def test_exclude_by_name(self):
        result = self._run_json(
            _make_grouped_candidates(),
            selected_subsystem=ALPHA_SUBSYSTEM,
            exclude=['alpha1'],
        )
        self.assertEqual({ALPHA_FILE_2}, self._files(result))

    def test_exclude_by_subsystem(self):
        result = self._run_json(
            _make_grouped_candidates(), exclude=[ALPHA_SUBSYSTEM]
        )
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual({BETA_FILE_1}, self._files(result))

    def test_excluded_selected_subsystem_is_reselected(self):
        result = self._run_json(
            _make_grouped_candidates(),
            selected_subsystem=ALPHA_SUBSYSTEM,
            exclude=[ALPHA_SUBSYSTEM],
        )
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(1, result['count'])
        self.assertEqual({BETA_FILE_1}, self._files(result))

    def _make_tiered_candidates(self, counts):
        """Returns candidates with counts[subsystem] files per subsystem."""
        return [
            {'file': f'{subsystem}/F{i}.java', 'name': f'{subsystem}{i}'}
            for subsystem, count in counts.items()
            for i in range(count)
        ]

    def test_prefers_subsystem_with_full_batch(self):
        candidates = self._make_tiered_candidates(
            {'components/a': 4, 'components/b': 3, 'components/c': 2}
        )
        output = _run_and_capture(
            candidate_finder.handle_grouped_mode,
            candidates,
            4,
            json_output=True,
        )
        result = json.loads(output)
        self.assertEqual('components/a', result['subsystem'])
        self.assertEqual(4, result['count'])

    def test_prefers_subsystem_with_three_candidates(self):
        candidates = self._make_tiered_candidates(
            {'components/a': 3, 'components/b': 2, 'components/c': 1}
        )
        result = self._run_json(candidates)
        self.assertEqual('components/a', result['subsystem'])
        self.assertEqual(3, result['count'])

    def test_shuffles_candidates(self):
        with mock.patch.object(candidate_finder.random, 'shuffle') as shuffle:
            self._run_json(
                _make_grouped_candidates(), selected_subsystem=ALPHA_SUBSYSTEM
            )
        shuffle.assert_called_once()

    def test_empty_json(self):
        result = self._run_json([])
        self.assertEqual('grouped', result['mode'])
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])

    def test_all_excluded_json(self):
        result = self._run_json(
            _make_grouped_candidates(),
            exclude=[ALPHA_SUBSYSTEM, BETA_SUBSYSTEM],
        )
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])

    def test_empty_plain_text(self):
        output = _run_and_capture(candidate_finder.handle_grouped_mode, [], 10)
        self.assertIn('No candidates found.', output)

    def test_json_preserves_banned_and_valid_lists(self):
        valid = [f'org.example.Valid{i}' for i in range(25)]
        banned = [f'org.example.Banned{i}' for i in range(25)]
        candidates = [{'file': ALPHA_FILE_1, 'valid': valid, 'banned': banned}]
        result = self._run_json(candidates)
        self.assertEqual(valid, result['candidates'][0]['valid'])
        self.assertEqual(banned, result['candidates'][0]['banned'])
        self.assertEqual(sorted(valid), result['metadata']['valid'])
        self.assertEqual(sorted(banned), result['metadata']['banned'])


class HandleAtomicModeTest(unittest.TestCase):
    def _run_json(self, candidates, exclude):
        plugin = types.SimpleNamespace(
            find_candidates=lambda repo_root: iter(candidates)
        )
        output = _run_and_capture(
            candidate_finder.handle_atomic_mode,
            plugin,
            10,
            '/repo',
            exclude=exclude,
            json_output=True,
        )
        return json.loads(output)

    def test_exclude_by_id_name_and_file(self):
        candidates = [
            {'id': 'id1', 'name': 'name1', 'file': 'f1.xml'},
            {'id': 'id2', 'name': 'name2', 'file': 'f2.xml'},
            {'id': 'id3', 'name': 'name3', 'file': 'f3.xml'},
            {'id': 'id4', 'name': 'name4', 'file': 'f4.xml'},
        ]
        result = self._run_json(candidates, ['id1', 'name2', 'f3.xml'])
        self.assertEqual(['id4'], [c['id'] for c in result['candidates']])

    def test_exclude_candidate_without_name(self):
        result = self._run_json([{'id': 'id1'}, {'id': 'id2'}], ['id1'])
        self.assertEqual(['id2'], [c['id'] for c in result['candidates']])

    def test_all_excluded_json(self):
        result = self._run_json([{'id': 'id1'}], ['id1'])
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])


class _SortAwarePlugin:
    """Atomic plugin that declares a sort keyword and sorts by name only.

    The handler must not pass sort, so sort_args records None per call.
    """

    def __init__(self, candidates):
        self.candidates = candidates
        self.sort_args = []
        self.yielded = 0

    def find_candidates(self, repo_root, sort=None):
        self.sort_args.append(sort)
        items = sorted(self.candidates, key=lambda c: c['name'].lower())
        for c in items:
            self.yielded += 1
            yield c


def _make_one_arg_plugin(candidates):
    """Atomic plugin whose find_candidates() only takes repo_root."""
    return types.SimpleNamespace(
        find_candidates=lambda repo_root: iter(candidates)
    )


def _make_named_candidates():
    return [
        {'name': 'Charlie', 'file': 'c.xml'},
        {'name': 'alpha', 'file': 'a.xml'},
        {'name': 'Delta', 'file': 'd.xml'},
        {'name': 'bravo', 'file': 'b.xml'},
    ]


def _make_cross_directory_candidates():
    # Name order (Alpha, Mid, Zebra) is the reverse of directory order.
    return [
        {'name': 'Mid', 'file': 'dir_b/m.xml'},
        {'name': 'Zebra', 'file': 'dir_a/z.xml'},
        {'name': 'Alpha', 'file': 'dir_c/a.xml'},
    ]


def _names(result):
    return [c['name'] for c in result['candidates']]


class HandleAtomicModeSortTest(unittest.TestCase):
    def _run_json(self, plugin, count=10, exclude=None, sort='random'):
        output = _run_and_capture(
            candidate_finder.handle_atomic_mode,
            plugin,
            count,
            '/repo',
            exclude=exclude,
            json_output=True,
            sort=sort,
        )
        return json.loads(output)

    def test_sort_aware_plugin_asc(self):
        plugin = _SortAwarePlugin(_make_named_candidates())
        result = self._run_json(plugin, count=3, sort='asc')
        self.assertEqual(['alpha', 'bravo', 'Charlie'], _names(result))
        self.assertEqual([None], plugin.sort_args)

    def test_sort_aware_plugin_desc(self):
        plugin = _SortAwarePlugin(_make_named_candidates())
        result = self._run_json(plugin, count=3, sort='desc')
        self.assertEqual(['Delta', 'Charlie', 'bravo'], _names(result))
        self.assertEqual([None], plugin.sort_args)

    def test_sort_aware_plugin_asc_orders_by_directory_first(self):
        # The plugin yields by name only: Alpha, Mid, Zebra.
        plugin = _SortAwarePlugin(_make_cross_directory_candidates())
        result = self._run_json(plugin, count=3, sort='asc')
        self.assertEqual(['Zebra', 'Mid', 'Alpha'], _names(result))
        self.assertEqual([None], plugin.sort_args)

    def test_sort_aware_plugin_desc_orders_by_directory_first(self):
        plugin = _SortAwarePlugin(_make_cross_directory_candidates())
        result = self._run_json(plugin, count=3, sort='desc')
        self.assertEqual(['Alpha', 'Mid', 'Zebra'], _names(result))
        self.assertEqual([None], plugin.sort_args)

    def test_sorted_modes_consume_all_candidates(self):
        for sort in ('asc', 'desc'):
            plugin = _SortAwarePlugin(_make_named_candidates())
            self._run_json(plugin, count=2, sort=sort)
            self.assertEqual(4, plugin.yielded)

    def test_sorted_modes_skip_exclusions_before_slicing_count(self):
        plugin = _SortAwarePlugin(_make_named_candidates())
        result = self._run_json(
            plugin, count=2, exclude=['alpha', 'b.xml'], sort='asc'
        )
        self.assertEqual(['Charlie', 'Delta'], _names(result))

    def test_sort_aware_plugin_random_does_not_receive_sort(self):
        plugin = _SortAwarePlugin(_make_named_candidates())
        result = self._run_json(plugin)
        self.assertEqual([None], plugin.sort_args)
        self.assertEqual(
            {'alpha', 'bravo', 'Charlie', 'Delta'}, set(_names(result))
        )

    def test_plugin_with_kwargs_does_not_receive_sort(self):
        received = []

        def find_candidates(*args, **kwargs):
            received.append((args, kwargs))
            return iter(_make_named_candidates())

        plugin = types.SimpleNamespace(find_candidates=find_candidates)
        self._run_json(plugin, count=1, sort='asc')
        self.assertEqual([(('/repo',), {})], received)

    def test_one_arg_plugin_asc_is_sorted_by_handler(self):
        plugin = _make_one_arg_plugin(_make_named_candidates())
        result = self._run_json(plugin, count=3, sort='asc')
        self.assertEqual(['alpha', 'bravo', 'Charlie'], _names(result))

    def test_one_arg_plugin_desc_is_sorted_by_handler(self):
        plugin = _make_one_arg_plugin(_make_named_candidates())
        result = self._run_json(plugin, count=3, sort='desc')
        self.assertEqual(['Delta', 'Charlie', 'bravo'], _names(result))

    def test_one_arg_plugin_sort_considers_all_candidates(self):
        # The best candidate comes last, so the handler can't stop early.
        candidates = [{'name': f'n{i:02d}'} for i in range(30, 0, -1)]
        plugin = _make_one_arg_plugin(candidates)
        result = self._run_json(plugin, count=2, sort='asc')
        self.assertEqual(['n01', 'n02'], _names(result))

    def test_one_arg_plugin_sort_orders_by_file_then_name_or_id(self):
        candidates = [
            {'file': 'z.xml'},
            {'id': 'm1'},
            {'name': 'a1', 'id': 'zz'},
            {'id': 'b1', 'file': 'a.xml'},
        ]
        plugin = _make_one_arg_plugin(candidates)
        result = self._run_json(plugin, count=4, sort='asc')
        # Candidates without a file have an empty path and sort first, ordered
        # by name or id; then the rest are ordered by file name.
        self.assertEqual(
            ['a1', 'm1', 'b1', 'z.xml'],
            [
                c.get('name') or c.get('id') or c['file']
                for c in result['candidates']
            ],
        )

    def test_one_arg_plugin_sort_orders_by_directory_before_name(self):
        candidates = [
            {'name': 'Alpha', 'file': 'dir_b/a.xml'},
            {'name': 'Zebra', 'file': 'dir_a/sub/z.xml'},
        ]
        plugin = _make_one_arg_plugin(candidates)
        self.assertEqual(
            ['Zebra', 'Alpha'], _names(self._run_json(plugin, sort='asc'))
        )
        self.assertEqual(
            ['Alpha', 'Zebra'], _names(self._run_json(plugin, sort='desc'))
        )

    def test_one_arg_plugin_sort_ties_in_same_file_broken_by_name(self):
        candidates = [
            {'name': 'beta', 'file': 'dir/a.xml'},
            {'name': 'Alpha', 'file': 'dir/a.xml'},
            {'name': 'alpha', 'file': 'dir/a.xml'},
        ]
        plugin = _make_one_arg_plugin(candidates)
        self.assertEqual(
            ['Alpha', 'alpha', 'beta'],
            _names(self._run_json(plugin, sort='asc')),
        )
        self.assertEqual(
            ['beta', 'alpha', 'Alpha'],
            _names(self._run_json(plugin, sort='desc')),
        )

    def test_one_arg_plugin_sort_orders_files_within_directory(self):
        candidates = [
            {'name': 'A', 'file': 'dir/b.xml'},
            {'name': 'Z', 'file': 'dir/a.xml'},
        ]
        plugin = _make_one_arg_plugin(candidates)
        self.assertEqual(['Z', 'A'], _names(self._run_json(plugin, sort='asc')))

    def test_one_arg_plugin_sort_compares_path_components(self):
        candidates = [
            {'name': 'n1', 'file': 'dir-other/a.xml'},
            {'name': 'n2', 'file': 'dir/sub/a.xml'},
            {'name': 'n3', 'file': 'dir/z.xml'},
        ]
        plugin = _make_one_arg_plugin(candidates)
        # 'dir' < 'dir/sub' < 'dir-other', even though '-' < '/' as characters.
        self.assertEqual(
            ['n3', 'n2', 'n1'], _names(self._run_json(plugin, sort='asc'))
        )
        self.assertEqual(
            ['n1', 'n2', 'n3'], _names(self._run_json(plugin, sort='desc'))
        )

    def test_one_arg_plugin_sort_ties_broken_case_sensitively(self):
        plugin = _make_one_arg_plugin([{'name': 'abc'}, {'name': 'ABC'}])
        self.assertEqual(
            ['ABC', 'abc'], _names(self._run_json(plugin, sort='asc'))
        )
        self.assertEqual(
            ['abc', 'ABC'], _names(self._run_json(plugin, sort='desc'))
        )

    def test_one_arg_plugin_sort_non_dict_candidates(self):
        plugin = _make_one_arg_plugin(['b', 'C', 'a'])
        with mock.patch.object(candidate_finder.random, 'sample') as sample:
            output = _run_and_capture(
                candidate_finder.handle_atomic_mode,
                plugin,
                2,
                '/repo',
                json_output=True,
                sort='desc',
            )
        sample.assert_not_called()
        self.assertEqual(['C', 'b'], json.loads(output)['candidates'])

    def test_one_arg_plugin_sort_excludes_non_dict_candidates(self):
        plugin = _make_one_arg_plugin(['b', 'C', 'a'])
        output = _run_and_capture(
            candidate_finder.handle_atomic_mode,
            plugin,
            5,
            '/repo',
            exclude=['C'],
            json_output=True,
            sort='asc',
        )
        self.assertEqual(['a', 'b'], json.loads(output)['candidates'])

    def test_sorted_modes_do_not_sample(self):
        for plugin in (
            _SortAwarePlugin(_make_named_candidates()),
            _make_one_arg_plugin(_make_named_candidates()),
        ):
            for sort in ('asc', 'desc'):
                with mock.patch.object(
                    candidate_finder.random, 'sample'
                ) as sample:
                    self._run_json(plugin, count=2, sort=sort)
                sample.assert_not_called()

    def test_sort_with_all_excluded_reports_no_candidates(self):
        plugin = _SortAwarePlugin([{'name': 'a'}])
        result = self._run_json(plugin, exclude=['a'], sort='asc')
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])

    def test_sort_plain_text_output_is_ordered(self):
        plugin = _make_one_arg_plugin(_make_named_candidates())
        output = _run_and_capture(
            candidate_finder.handle_atomic_mode,
            plugin,
            4,
            '/repo',
            sort='asc',
        )
        positions = [
            output.index(f'Name: {n}')
            for n in ('alpha', 'bravo', 'Charlie', 'Delta')
        ]
        self.assertEqual(sorted(positions), positions)


class HandleAtomicModeRelatedHistogramsTest(unittest.TestCase):
    def _run_json(self, candidates, exclude, sort='random'):
        output = _run_and_capture(
            candidate_finder.handle_atomic_mode,
            _make_one_arg_plugin(candidates),
            10,
            '/repo',
            exclude=exclude,
            json_output=True,
            sort=sort,
        )
        return json.loads(output)

    def _candidate(self):
        return {'name': 'H.A.One', 'related_histograms': ['b', 'c', 'd']}

    def test_excluded_related_histograms_are_filtered(self):
        for sort in ('random', 'asc', 'desc'):
            result = self._run_json([self._candidate()], ['c'], sort)
            self.assertEqual(
                ['b', 'd'], result['candidates'][0]['related_histograms']
            )

    def test_related_histograms_removed_when_all_excluded(self):
        for sort in ('random', 'asc'):
            result = self._run_json([self._candidate()], ['b', 'c', 'd'], sort)
            self.assertEqual(1, result['count'])
            self.assertNotIn('related_histograms', result['candidates'][0])

    def test_related_histograms_untouched_without_matching_exclude(self):
        result = self._run_json([self._candidate()], ['unrelated'])
        self.assertEqual(
            ['b', 'c', 'd'], result['candidates'][0]['related_histograms']
        )

    def test_filtering_does_not_mutate_plugin_candidates(self):
        candidate = self._candidate()
        self._run_json([candidate], ['c', 'b', 'd'])
        self.assertEqual(['b', 'c', 'd'], candidate['related_histograms'])

    def test_candidates_without_related_histograms_are_unchanged(self):
        result = self._run_json([{'name': 'x', 'summary': 's'}], ['c'])
        self.assertEqual([{'name': 'x', 'summary': 's'}], result['candidates'])


class LoadExcludeFileTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)

    def _write(self, name, content):
        path = os.path.join(self._tmp.name, name)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        return path

    def test_grouped_json_collects_uploaded_and_skipped_keys(self):
        path = self._write(
            'ledger.json',
            json.dumps(
                {
                    'uploaded': {'h1': {'cl': 1}, 'h2': {'cl': 2}},
                    'skipped': {'h3': 'reason'},
                    'other': {'h4': 1},
                }
            ),
        )
        self.assertEqual(
            ['h1', 'h2', 'h3'], candidate_finder._load_exclude_file(path)
        )

    def test_grouped_json_with_only_one_section(self):
        path = self._write('ledger.json', json.dumps({'skipped': {'h3': 1}}))
        self.assertEqual(['h3'], candidate_finder._load_exclude_file(path))

    def test_grouped_json_ignores_non_dict_sections(self):
        path = self._write(
            'ledger.json',
            json.dumps({'uploaded': ['h1'], 'skipped': {'h2': 1}}),
        )
        self.assertEqual(['h2'], candidate_finder._load_exclude_file(path))

    def test_flat_json_uses_top_level_keys(self):
        path = self._write('ledger.json', json.dumps({'h1': 1, 'h2': {}}))
        self.assertEqual(
            ['h1', 'h2'], candidate_finder._load_exclude_file(path)
        )

    def test_text_file_skips_blank_lines_and_comments(self):
        path = self._write(
            'exclude.txt',
            '# Already uploaded\nh1\n\n  h2  \n   \n# h3\nh4 # not a comment\n',
        )
        self.assertEqual(
            ['h1', 'h2', 'h4 # not a comment'],
            candidate_finder._load_exclude_file(path),
        )

    def test_empty_text_file(self):
        path = self._write('exclude.txt', '')
        self.assertEqual([], candidate_finder._load_exclude_file(path))

    def test_malformed_json_raises_value_error(self):
        path = self._write('ledger.json', '{"uploaded": ')
        with self.assertRaises(ValueError):
            candidate_finder._load_exclude_file(path)

    def test_non_dict_json_raises_value_error(self):
        path = self._write('ledger.json', json.dumps(['h1', 'h2']))
        with self.assertRaises(ValueError):
            candidate_finder._load_exclude_file(path)

    def test_missing_file_raises_os_error(self):
        for name in ('missing.json', 'missing.txt'):
            with self.assertRaises(OSError):
                candidate_finder._load_exclude_file(
                    os.path.join(self._tmp.name, name)
                )


class MainAtomicTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.plugin = _SortAwarePlugin(_make_named_candidates())
        for p in (
            mock.patch.object(
                candidate_finder,
                'load_plugin_from_path',
                side_effect=lambda path: self.plugin,
            ),
            mock.patch.object(
                hub_utils, 'find_repo_root', return_value='/repo'
            ),
        ):
            p.start()
            self.addCleanup(p.stop)

    def _write(self, name, content):
        path = os.path.join(self._tmp.name, name)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        return path

    def _run_main(self, *extra_args):
        argv = [
            'candidate_finder.py',
            'find',
            '--plugin',
            'plugin.py',
            '--json',
            '--count',
            '10',
        ]
        argv.extend(extra_args)
        out = io.StringIO()
        with (
            mock.patch.object(sys, 'argv', argv),
            contextlib.redirect_stdout(out),
            contextlib.redirect_stderr(io.StringIO()),
        ):
            candidate_finder.main()
        return json.loads(out.getvalue())

    def test_sort_asc_with_sort_aware_plugin(self):
        result = self._run_main('--sort', 'asc', '--count', '3')
        self.assertEqual(['alpha', 'bravo', 'Charlie'], _names(result))
        self.assertEqual([None], self.plugin.sort_args)

    def test_sort_desc_with_sort_aware_plugin(self):
        result = self._run_main('--sort', 'desc', '--count', '2')
        self.assertEqual(['Delta', 'Charlie'], _names(result))
        self.assertEqual([None], self.plugin.sort_args)

    def test_sort_asc_orders_by_directory_over_plugin_name_order(self):
        self.plugin = _SortAwarePlugin(_make_cross_directory_candidates())
        result = self._run_main('--sort', 'asc', '--count', '2')
        self.assertEqual(['Zebra', 'Mid'], _names(result))
        self.assertEqual([None], self.plugin.sort_args)

    def test_sort_desc_orders_by_directory_over_plugin_name_order(self):
        self.plugin = _SortAwarePlugin(_make_cross_directory_candidates())
        result = self._run_main('--sort', 'desc', '--count', '2')
        self.assertEqual(['Alpha', 'Mid'], _names(result))
        self.assertEqual([None], self.plugin.sort_args)

    def test_sort_asc_with_one_arg_plugin(self):
        self.plugin = _make_one_arg_plugin(_make_named_candidates())
        result = self._run_main('--sort', 'asc', '--count', '3')
        self.assertEqual(['alpha', 'bravo', 'Charlie'], _names(result))

    def test_sort_desc_with_one_arg_plugin(self):
        self.plugin = _make_one_arg_plugin(_make_named_candidates())
        result = self._run_main('--sort', 'desc', '--count', '2')
        self.assertEqual(['Delta', 'Charlie'], _names(result))

    def test_default_sort_is_random(self):
        self._run_main()
        self.assertEqual([None], self.plugin.sort_args)

    def test_invalid_sort_exits_non_zero(self):
        with self.assertRaises(SystemExit) as cm:
            self._run_main('--sort', 'sideways')
        self.assertNotEqual(0, cm.exception.code)

    def test_exclude_file_grouped_json(self):
        path = self._write(
            'ledger.json',
            json.dumps(
                {
                    'uploaded': {'alpha': {}, 'Charlie': {}},
                    'skipped': {'Delta': {}},
                }
            ),
        )
        result = self._run_main('--sort', 'asc', '--exclude-file', path)
        self.assertEqual(['bravo'], _names(result))

    def test_exclude_file_flat_json(self):
        path = self._write('ledger.json', json.dumps({'alpha': 1, 'bravo': 2}))
        result = self._run_main('--sort', 'asc', '--exclude-file', path)
        self.assertEqual(['Charlie', 'Delta'], _names(result))

    def test_exclude_file_text_with_comments_and_blank_lines(self):
        path = self._write(
            'exclude.txt', '# Uploaded\nalpha\n\n  Delta  \n# bravo\n'
        )
        result = self._run_main('--sort', 'asc', '--exclude-file', path)
        self.assertEqual(['bravo', 'Charlie'], _names(result))

    def test_exclude_file_matches_file_paths(self):
        path = self._write('exclude.txt', 'a.xml\n')
        result = self._run_main('--sort', 'asc', '--exclude-file', path)
        self.assertEqual(['bravo', 'Charlie', 'Delta'], _names(result))

    def test_multiple_exclude_files_and_exclude_flag_accumulate(self):
        text_path = self._write('exclude.txt', 'alpha\n')
        json_path = self._write('ledger.json', json.dumps({'bravo': 1}))
        result = self._run_main(
            '--sort',
            'asc',
            '--exclude-file',
            text_path,
            '--exclude-file',
            json_path,
            '--exclude',
            'Charlie',
        )
        self.assertEqual(['Delta'], _names(result))

    def test_exclude_file_filters_related_histograms(self):
        self.plugin = _SortAwarePlugin(
            [
                {
                    'name': 'H.A.One',
                    'related_histograms': ['H.A.Two', 'H.A.Three'],
                },
                {'name': 'H.B.One'},
            ]
        )
        path = self._write(
            'ledger.json', json.dumps({'uploaded': {'H.A.Two': {}}})
        )
        result = self._run_main(
            '--sort', 'asc', '--exclude-file', path, '--count', '2'
        )
        self.assertEqual(['H.A.One', 'H.B.One'], _names(result))
        self.assertEqual(
            ['H.A.Three'], result['candidates'][0]['related_histograms']
        )

    def test_malformed_json_exclude_file_fails(self):
        path = self._write('ledger.json', '{not json')
        with self.assertRaises(ValueError):
            self._run_main('--exclude-file', path)
        self.assertEqual([], self.plugin.sort_args)

    def test_non_dict_json_exclude_file_fails(self):
        path = self._write('ledger.json', json.dumps(['alpha']))
        with self.assertRaises(ValueError):
            self._run_main('--exclude-file', path)

    def test_missing_exclude_file_fails(self):
        with self.assertRaises(OSError):
            self._run_main(
                '--exclude-file', os.path.join(self._tmp.name, 'nope.txt')
            )


class MainGroupedTest(unittest.TestCase):
    def setUp(self):
        self.checked_files = []
        self.candidate_names = {
            ALPHA_FILE_1: 'alpha1',
            ALPHA_FILE_2: 'alpha2',
            BETA_FILE_1: 'beta1',
        }
        self.plugin = types.SimpleNamespace(
            MODE='grouped',
            FILE_EXTENSIONS=['.java'],
            check_file=self._check_file,
        )
        self.dirs_with_files = [
            ('chrome/browser/alpha', [ALPHA_FILE_1, ALPHA_FILE_2]),
            ('components/beta', [BETA_FILE_1]),
        ]
        self.patchers = [
            mock.patch.object(
                candidate_finder,
                'load_plugin_from_path',
                return_value=self.plugin,
            ),
            mock.patch.object(
                candidate_finder,
                'get_files',
                side_effect=lambda *a, **k: list(self.dirs_with_files),
            ),
            mock.patch.object(
                hub_utils, 'find_repo_root', return_value='/repo'
            ),
        ]
        for p in self.patchers:
            p.start()
            self.addCleanup(p.stop)

    def _check_file(self, filepath, root):
        self.checked_files.append(filepath)
        name = self.candidate_names.get(filepath)
        if not name:
            return None
        return {
            'name': name,
            'valid': [f'valid.{i}' for i in range(15)],
            'banned': [f'banned.{i}' for i in range(15)],
        }

    def _run_main(self, *extra_args):
        argv = [
            'candidate_finder.py',
            'find',
            '--plugin',
            'plugin.py',
            '--json',
        ]
        argv.extend(extra_args)
        out = io.StringIO()
        with (
            mock.patch.object(sys, 'argv', argv),
            contextlib.redirect_stdout(out),
            contextlib.redirect_stderr(io.StringIO()),
        ):
            candidate_finder.main()
        return json.loads(out.getvalue())

    def test_exclude_by_file(self):
        result = self._run_main('--exclude', ALPHA_FILE_1, ALPHA_FILE_2)
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(
            [BETA_FILE_1], [c['file'] for c in result['candidates']]
        )

    def test_exclude_by_name(self):
        result = self._run_main('--exclude', 'alpha1', 'alpha2')
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(
            [BETA_FILE_1], [c['file'] for c in result['candidates']]
        )

    def test_repeated_exclude_flags_accumulate(self):
        result = self._run_main(
            '--exclude', ALPHA_FILE_1, '--exclude', ALPHA_FILE_2
        )
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(
            [BETA_FILE_1], [c['file'] for c in result['candidates']]
        )

    def test_exclude_by_subsystem_skips_scanning(self):
        result = self._run_main('--exclude', ALPHA_SUBSYSTEM)
        self.assertEqual(BETA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(
            [BETA_FILE_1], [c['file'] for c in result['candidates']]
        )
        self.assertNotIn(ALPHA_FILE_1, self.checked_files)
        self.assertNotIn(ALPHA_FILE_2, self.checked_files)

    def test_partial_file_exclude_keeps_subsystem(self):
        result = self._run_main('--exclude', ALPHA_FILE_1, BETA_FILE_1)
        self.assertEqual(ALPHA_SUBSYSTEM, result['subsystem'])
        self.assertEqual(
            [ALPHA_FILE_2], [c['file'] for c in result['candidates']]
        )

    def test_empty_result_json(self):
        result = self._run_main('--exclude', ALPHA_SUBSYSTEM, BETA_SUBSYSTEM)
        self.assertEqual('grouped', result['mode'])
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])

    def test_no_candidates_json(self):
        self.candidate_names.clear()
        result = self._run_main()
        self.assertEqual(0, result['count'])
        self.assertEqual([], result['candidates'])

    def test_json_preserves_banned_and_valid_lists(self):
        result = self._run_main('--exclude', ALPHA_SUBSYSTEM)
        candidate = result['candidates'][0]
        self.assertEqual(15, len(candidate['valid']))
        self.assertEqual(15, len(candidate['banned']))
        self.assertEqual(15, len(result['metadata']['valid']))
        self.assertEqual(15, len(result['metadata']['banned']))

    def test_sort_defaults_to_random(self):
        result = self._run_main()
        candidate_finder.get_files.assert_called_once()
        self.assertEqual('grouped', result['mode'])
        self.assertGreater(result['count'], 0)

    def test_sort_random_is_accepted_for_grouped_plugins(self):
        result = self._run_main('--sort', 'random')
        candidate_finder.get_files.assert_called_once()
        self.assertEqual('grouped', result['mode'])
        self.assertGreater(result['count'], 0)

    def test_sort_fails_for_grouped_mode(self):
        self.plugin.initialize = mock.Mock()
        for sort in ('asc', 'desc'):
            with self.subTest(sort=sort):
                with self.assertRaises(SystemExit) as cm:
                    self._run_main('--sort', sort)
                self.assertNotEqual(0, cm.exception.code)
                candidate_finder.get_files.assert_not_called()
                self.plugin.initialize.assert_not_called()
                self.assertEqual([], self.checked_files)


class GetFilesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.repo = os.path.realpath(self._tmp.name)
        self._orig_cwd = os.getcwd()
        self.addCleanup(os.chdir, self._orig_cwd)

    def _touch(self, rel_path):
        path = os.path.join(self.repo, rel_path)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w'):
            pass

    def _flatten(self, result):
        return sorted(f for _, files in result for f in files)

    def _mock_git(self, stdout):
        return mock.patch.object(
            candidate_finder.subprocess,
            'run',
            return_value=types.SimpleNamespace(stdout=stdout),
        )

    def test_cwd_differs_from_search_root(self):
        self._touch('chrome/browser/a.java')
        self._touch('chrome/browser/ui/b.java')
        os.chdir(os.path.join(self.repo, 'chrome', 'browser', 'ui'))
        git_output = 'chrome/browser/a.java\nchrome/browser/ui/b.java\n'
        with self._mock_git(git_output) as run_mock:
            result = candidate_finder.get_files(self.repo, ['.java'])
        self.assertEqual(self.repo, run_mock.call_args.kwargs['cwd'])
        self.assertEqual(
            ['chrome/browser/a.java', 'chrome/browser/ui/b.java'],
            self._flatten(result),
        )
        self.assertEqual(
            {'chrome/browser', 'chrome/browser/ui'},
            {directory for directory, _ in result},
        )
        for _, files in result:
            for f in files:
                self.assertFalse(f.startswith('..'))

    def test_relative_search_root_returns_paths_relative_to_it(self):
        self._touch('chrome/browser/a.java')
        self._touch('chrome/browser/ui/b.java')
        os.chdir(self.repo)
        search_root = os.path.join('chrome', 'browser')
        # git ls-files run with cwd=search_root lists paths relative to it.
        with self._mock_git('a.java\nui/b.java\n') as run_mock:
            result = candidate_finder.get_files(search_root, ['.java'])
        self.assertEqual(
            os.path.join(self.repo, 'chrome', 'browser'),
            run_mock.call_args.kwargs['cwd'],
        )
        self.assertEqual(['a.java', 'ui/b.java'], self._flatten(result))
        self.assertEqual({'', 'ui'}, {directory for directory, _ in result})

    def test_git_ls_files_filters_extensions_and_skip_dirs(self):
        git_output = (
            'chrome/a.java\n'
            'chrome/a.cc\n'
            'chrome/skipme/b.java\n'
            'third_party/c.java\n'
        )
        with self._mock_git(git_output):
            result = candidate_finder.get_files(
                self.repo, ['.java'], plugin_skip_dirs={'skipme'}
            )
        self.assertEqual(['chrome/a.java'], self._flatten(result))

    def test_os_walk_fallback_returns_paths_relative_to_search_root(self):
        self._touch('chrome/browser/a.java')
        self._touch('chrome/browser/ui/b.java')
        self._touch('chrome/browser/ui/c.cc')
        self._touch('chrome/browser/skipme/d.java')
        self._touch('chrome/browser/third_party/e.java')
        os.chdir(os.path.join(self.repo, 'chrome', 'browser', 'ui'))
        with mock.patch.object(
            candidate_finder.subprocess,
            'run',
            side_effect=subprocess.CalledProcessError(128, 'git'),
        ):
            result = candidate_finder.get_files(
                self.repo, ['.java'], plugin_skip_dirs={'skipme'}
            )
        self.assertEqual(
            ['chrome/browser/a.java', 'chrome/browser/ui/b.java'],
            self._flatten(result),
        )

    def test_os_walk_fallback_with_relative_search_root(self):
        self._touch('chrome/browser/a.java')
        self._touch('chrome/browser/ui/b.java')
        self._touch('chrome/browser/skipme/d.java')
        os.chdir(self.repo)
        with mock.patch.object(
            candidate_finder.subprocess,
            'run',
            side_effect=FileNotFoundError('git'),
        ):
            result = candidate_finder.get_files(
                os.path.join('chrome', 'browser'),
                ['.java'],
                plugin_skip_dirs={'skipme'},
            )
        self.assertEqual(['a.java', 'ui/b.java'], self._flatten(result))

    _GIT_OUTPUT = 'b/z.java\na/B.java\na/a.java\na-x/c.java\na/sub/d.java\n'

    def test_shuffles_directories_and_files(self):
        with self._mock_git(self._GIT_OUTPUT):
            with mock.patch.object(
                candidate_finder.random, 'shuffle'
            ) as shuffle:
                result = candidate_finder.get_files(self.repo, ['.java'])
        # Once for the directories plus once per directory's files.
        self.assertEqual(5, shuffle.call_count)
        self.assertEqual(
            ['a-x/c.java', 'a/B.java', 'a/a.java', 'a/sub/d.java', 'b/z.java'],
            self._flatten(result),
        )

    def test_keeps_all_files(self):
        with self._mock_git(self._GIT_OUTPUT):
            result = candidate_finder.get_files(self.repo, ['.java'])
        self.assertEqual(
            {'a', 'a/sub', 'a-x', 'b'}, {directory for directory, _ in result}
        )
        self.assertEqual(5, len(self._flatten(result)))


if __name__ == '__main__':
    unittest.main()

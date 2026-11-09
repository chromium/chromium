# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for format_and_lint.py."""

import contextlib
import io
import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import format_and_lint
import hub_utils

_HIST_DIR = 'tools/metrics/histograms'
_TAB_HISTOGRAMS = f'{_HIST_DIR}/metadata/tab/histograms.xml'
_TAB_ENUMS = f'{_HIST_DIR}/metadata/tab/enums.xml'
_ENUMS = f'{_HIST_DIR}/enums.xml'
_VARIANTS = f'{_HIST_DIR}/variants.xml'
_LINT_CMD = ['vpython3', f'{_HIST_DIR}/validate_format.py']


class FormatAndLintTest(unittest.TestCase):
    def setUp(self):
        self.results = {}
        self.calls = []
        run_patcher = mock.patch.object(
            hub_utils, 'run_cmd', side_effect=self._fake_run
        )
        run_patcher.start()
        self.addCleanup(run_patcher.stop)
        root_patcher = mock.patch.object(
            hub_utils, 'find_repo_root', return_value='/repo'
        )
        root_patcher.start()
        self.addCleanup(root_patcher.stop)

    def _fake_run(self, cmd, cwd=None):
        self.calls.append(cmd)
        self.assertEqual('/repo', cwd)
        if cmd[:2] == ['git', 'cl']:
            return self.results.get('format', (0, ''))
        if cmd[:2] == ['git', 'merge-base']:
            return self.results.get('merge-base', (0, 'abc123\n'))
        if 'diff' in cmd:
            return self.results.get('diff', (0, ''))
        if cmd == _LINT_CMD:
            return self.results.get('lint', (0, ''))
        self.fail(f'Unexpected command: {cmd}')

    def _run_main(self, argv=()):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = format_and_lint.main(list(argv))
        return rc, out.getvalue(), err.getvalue()

    def _set_changed(self, *paths):
        self.results['diff'] = (0, ''.join(f'{p}\0' for p in paths))

    def test_runs_git_cl_format_and_prints_output(self):
        self.results['format'] = (0, 'Formatted.\n')
        rc, out, err = self._run_main()
        self.assertEqual(0, rc)
        self.assertEqual('Formatted.\n', out)
        self.assertEqual('', err)
        self.assertEqual(['git', 'cl', 'format'], self.calls[0])

    def test_silent_when_format_output_empty(self):
        rc, out, err = self._run_main()
        self.assertEqual((0, '', ''), (rc, out, err))

    def test_skips_lint_for_unrelated_files(self):
        self._set_changed(
            'A.java',
            'b.cc',
            f'{_HIST_DIR}/other/foo.xml',
            'chrome/android/res/layout/a.xml',
        )
        rc, _, _ = self._run_main()
        self.assertEqual(0, rc)
        self.assertNotIn(_LINT_CMD, self.calls)

    def test_skips_lint_when_diff_fails(self):
        self.results['diff'] = (1, 'fatal')
        rc, _, _ = self._run_main()
        self.assertEqual(0, rc)
        self.assertNotIn(_LINT_CMD, self.calls)

    def test_lint_triggered_for_histogram_xml(self):
        for path in (_TAB_HISTOGRAMS, _TAB_ENUMS, _ENUMS, _VARIANTS):
            with self.subTest(path=path):
                self.calls.clear()
                self._set_changed('A.java', path)
                rc, _, _ = self._run_main()
                self.assertEqual(0, rc)
                self.assertIn(_LINT_CMD, self.calls)

    def test_lint_triggered_for_deleted_histogram_xml(self):
        # `git diff --name-only` lists deleted paths too.
        self._set_changed(_TAB_HISTOGRAMS)
        rc, _, _ = self._run_main()
        self.assertEqual(0, rc)
        self.assertIn(_LINT_CMD, self.calls)

    def test_lint_output_passed_through(self):
        self._set_changed(_ENUMS)
        self.results['lint'] = (0, 'Histograms OK\n')
        rc, out, _ = self._run_main()
        self.assertEqual(0, rc)
        self.assertEqual('Histograms OK\n', out)

    def test_diff_uses_merge_base(self):
        self._run_main()
        diff_cmd = next(c for c in self.calls if 'diff' in c)
        self.assertEqual('abc123', diff_cmd[-1])
        self.assertIn('-z', diff_cmd)

    def test_diff_falls_back_to_head_without_merge_base(self):
        self.results['merge-base'] = (1, 'fatal: no merge base')
        self._run_main()
        diff_cmd = next(c for c in self.calls if 'diff' in c)
        self.assertEqual('HEAD', diff_cmd[-1])

    def test_format_failure_skips_lint(self):
        self._set_changed(_ENUMS)
        self.results['format'] = (3, 'format broke\n')
        rc, out, err = self._run_main()
        self.assertEqual(3, rc)
        self.assertEqual('', out)
        self.assertEqual('format broke\n', err)
        self.assertNotIn(_LINT_CMD, self.calls)

    def test_lint_failure_propagates(self):
        self._set_changed(_ENUMS)
        self.results['lint'] = (2, 'bad histogram\n')
        rc, out, err = self._run_main()
        self.assertEqual(2, rc)
        self.assertEqual('', out)
        self.assertEqual('bad histogram\n', err)

    def test_is_histogram_xml(self):
        self.assertTrue(format_and_lint.is_histogram_xml(_TAB_ENUMS))
        self.assertTrue(format_and_lint.is_histogram_xml(_VARIANTS))
        self.assertFalse(
            format_and_lint.is_histogram_xml(
                f'{_HIST_DIR}/metadata/histograms.xml'
            )
        )
        self.assertFalse(format_and_lint.is_histogram_xml(f'x/{_ENUMS}'))

    def test_rejects_unknown_args(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                format_and_lint.main(['--json'])


if __name__ == '__main__':
    unittest.main()

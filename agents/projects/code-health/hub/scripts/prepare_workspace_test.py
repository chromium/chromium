# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for prepare_workspace.py."""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hub_utils
import prepare_workspace

_PULL = ['git', 'pull', 'origin', 'main', '--rebase']
_SYNC = ['gclient', 'sync', '-D']
_STASH = ['git', 'stash', 'push', '-m', prepare_workspace.STASH_MESSAGE]
_CHECKOUT = ['git', 'checkout', 'main']


class PrepareWorkspaceTest(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.dirty = ''
        self.unmerged = ''
        self.failures = {}  # Maps a command tuple to (returncode, output).
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = tmp.name
        for name, replacement in (
            ('run_cmd', self._run_cmd),
            ('find_repo_root', lambda: self.repo),
        ):
            patcher = mock.patch.object(hub_utils, name, replacement)
            patcher.start()
            self.addCleanup(patcher.stop)

    def _run_cmd(self, cmd, cwd=None):
        self.calls.append(cmd)
        if tuple(cmd) in self.failures:
            return self.failures[tuple(cmd)]
        if cmd[:2] == ['git', 'status']:
            return 0, self.dirty
        if cmd[:2] == ['git', 'log']:
            return 0, self.unmerged
        return 0, ''

    def _main(self, argv=()):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = prepare_workspace.main(list(argv))
        return rc, out.getvalue(), err.getvalue()

    def test_clean_repo_checks_out_pulls_and_syncs(self):
        rc, out, _ = self._main()
        self.assertEqual(rc, 0)
        self.assertEqual(out, 'Workspace ready on main.\n')
        self.assertEqual(
            self.calls[1:],
            [
                _CHECKOUT,
                ['git', 'log', 'origin/main..HEAD', '--oneline'],
                _PULL,
                _SYNC,
            ],
        )

    def test_dirty_without_stash_refuses(self):
        self.dirty = ' M foo.cc'
        rc, _, err = self._main()
        self.assertEqual(rc, 1)
        self.assertIn('Uncommitted changes detected', err)
        self.assertNotIn(_CHECKOUT, self.calls)

    def test_dirty_with_stash_stashes_pulls_and_syncs(self):
        self.dirty = ' M foo.cc'
        rc, out, _ = self._main(['--stash'])
        self.assertEqual(rc, 0)
        self.assertIn('(stashed local changes)', out)
        self.assertLess(self.calls.index(_STASH), self.calls.index(_CHECKOUT))
        self.assertIn(_PULL, self.calls)
        self.assertIn(_SYNC, self.calls)

    def test_unmerged_commits_on_main_fail_before_pull(self):
        self.unmerged = 'abc123 Local commit'
        rc, _, err = self._main()
        self.assertEqual(rc, 1)
        self.assertIn('Unmerged local commits detected', err)
        self.assertIn('abc123 Local commit', err)
        self.assertNotIn(_PULL, self.calls)

    def test_pull_failure_propagates_returncode(self):
        self.failures[tuple(_PULL)] = (128, 'conflict')
        rc, _, err = self._main()
        self.assertEqual(rc, 128)
        self.assertIn('Command failed: git pull origin main --rebase', err)
        self.assertIn('conflict', err)
        self.assertNotIn(_SYNC, self.calls)

    def test_sync_failure_propagates_returncode(self):
        self.failures[tuple(_SYNC)] = (2, 'sync broke')
        rc, _, err = self._main()
        self.assertEqual(rc, 2)
        self.assertIn('sync broke', err)

    def test_missing_repo_directory_fails(self):
        missing = os.path.join(self.repo, 'missing')
        with self.assertRaisesRegex(
            prepare_workspace.StepError, 'Repository root not found'
        ):
            prepare_workspace.prepare_workspace(missing)
        self.assertEqual(self.calls, [])


if __name__ == '__main__':
    unittest.main()

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for hub_utils.py."""

import os
import subprocess
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hub_utils


def _completed(returncode, stdout, stderr):
    return mock.Mock(returncode=returncode, stdout=stdout, stderr=stderr)


class RunCmdTest(unittest.TestCase):
    @mock.patch('subprocess.run')
    def testSuccessIgnoresStderr(self, mock_run):
        mock_run.return_value = _completed(0, 'out\n', 'warning')
        rc, out = hub_utils.run_cmd(['foo', 'bar'], cwd='/some/dir')
        self.assertEqual(0, rc)
        self.assertEqual('out\n', out)
        _, kwargs = mock_run.call_args
        self.assertEqual(subprocess.PIPE, kwargs['stdout'])
        self.assertEqual(subprocess.PIPE, kwargs['stderr'])
        self.assertEqual('/some/dir', kwargs['cwd'])
        self.assertFalse(kwargs['check'])

    @mock.patch('subprocess.run')
    def testSuccessNoneStdout(self, mock_run):
        mock_run.return_value = _completed(0, None, '')
        self.assertEqual((0, ''), hub_utils.run_cmd(['foo']))

    @mock.patch('subprocess.run')
    def testFailureJoinsStdoutAndStderr(self, mock_run):
        mock_run.return_value = _completed(2, ' out \n', ' err \n')
        self.assertEqual((2, 'out\nerr'), hub_utils.run_cmd(['foo']))

    @mock.patch('subprocess.run')
    def testFailureStderrOnly(self, mock_run):
        mock_run.return_value = _completed(1, '', 'err\n')
        self.assertEqual((1, 'err'), hub_utils.run_cmd(['foo']))

    @mock.patch('subprocess.run')
    def testFailureEmptyOutput(self, mock_run):
        mock_run.return_value = _completed(3, None, None)
        self.assertEqual((3, ''), hub_utils.run_cmd(['foo']))

    @mock.patch('subprocess.run')
    def testOSErrorReturns127(self, mock_run):
        mock_run.side_effect = OSError('not found')
        rc, out = hub_utils.run_cmd(['missing_tool'])
        self.assertEqual(127, rc)
        self.assertEqual('Failed to run missing_tool: not found', out)


class FindRepoRootTest(unittest.TestCase):
    @mock.patch('hub_utils.run_cmd')
    def testReturnsGitRoot(self, mock_run_cmd):
        mock_run_cmd.return_value = (0, '/repo/root\n')
        self.assertEqual('/repo/root', hub_utils.find_repo_root('/repo/sub'))
        mock_run_cmd.assert_called_once_with(
            ['git', 'rev-parse', '--show-toplevel'], cwd='/repo/sub'
        )

    @mock.patch('hub_utils.run_cmd')
    def testFallsBackToStartOnFailure(self, mock_run_cmd):
        mock_run_cmd.return_value = (128, 'fatal: not a git repository')
        self.assertEqual('/tmp/x', hub_utils.find_repo_root('/tmp/x'))

    @mock.patch('hub_utils.run_cmd')
    def testFallsBackToStartOnEmptyOutput(self, mock_run_cmd):
        mock_run_cmd.return_value = (0, '  \n')
        self.assertEqual('/tmp/x', hub_utils.find_repo_root('/tmp/x'))

    @mock.patch('hub_utils.run_cmd')
    def testDefaultsToCwd(self, mock_run_cmd):
        mock_run_cmd.return_value = (1, '')
        self.assertEqual(os.getcwd(), hub_utils.find_repo_root())


if __name__ == '__main__':
    unittest.main()

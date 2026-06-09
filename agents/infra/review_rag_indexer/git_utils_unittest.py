#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for git_utils.py."""

import subprocess
import unittest
from unittest import mock

import git_utils


class ReadFilesAtRevisionTest(unittest.TestCase):

    def setUp(self):
        self.patcher = mock.patch('subprocess.run')
        self.mock_run = self.patcher.start()
        self.addCleanup(mock.patch.stopall)

    def test_read_files_success(self):
        mock_result = mock.Mock()
        self.mock_run.return_value = mock_result

        stdout_content = (b'filename1 blob 12\nfile1content\n'
                          b'filename2 blob 12\nfile2content\n')
        mock_result.stdout = stdout_content
        mock_result.stderr = b''
        mock_result.returncode = 0

        paths = ['path/to/file1', 'path/to/file2']
        results = git_utils.read_files_at_revision('rev1', paths)

        self.assertEqual(results, {
            'path/to/file1': 'file1content',
            'path/to/file2': 'file2content'
        })
        self.mock_run.assert_called_once_with(
            ['git', 'cat-file', '--batch'],
            input=b'rev1:path/to/file1\nrev1:path/to/file2\n',
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            text=False,
        )

    def test_read_files_missing(self):
        mock_result = mock.Mock()
        self.mock_run.return_value = mock_result

        stdout_content = b'rev1:path/to/file1 missing\n'
        mock_result.stdout = stdout_content
        mock_result.stderr = b''
        mock_result.returncode = 0

        results = git_utils.read_files_at_revision('rev1', ['path/to/file1'])
        self.assertEqual(results, {'path/to/file1': None})
        self.mock_run.assert_called_once_with(
            ['git', 'cat-file', '--batch'],
            input=b'rev1:path/to/file1\n',
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            text=False,
        )

    def test_read_files_subprocess_error(self):
        self.mock_run.side_effect = OSError('Subprocess failed')

        with self.assertRaises(OSError):
            git_utils.read_files_at_revision('rev1', ['path/to/file1'])

    def test_read_files_nonzero_exit(self):
        self.mock_run.side_effect = subprocess.CalledProcessError(
            returncode=1, cmd=['git', 'cat-file', '--batch'])

        with self.assertRaises(subprocess.CalledProcessError):
            git_utils.read_files_at_revision('rev1', ['path/to/file1'])


class RevisionExistsTest(unittest.TestCase):

    def setUp(self):
        self.patcher = mock.patch('subprocess.run')
        self.mock_run = self.patcher.start()
        self.addCleanup(mock.patch.stopall)

    def test_revision_exists_true(self):
        self.mock_run.return_value = mock.Mock(returncode=0)
        self.assertTrue(git_utils.revision_exists('valid_rev'))
        self.mock_run.assert_called_once_with(
            ['git', 'rev-parse', '--verify', 'valid_rev'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )

    def test_revision_exists_false(self):
        self.mock_run.side_effect = subprocess.CalledProcessError(
            returncode=1, cmd=['git', 'rev-parse', '--verify'])
        self.assertFalse(git_utils.revision_exists('invalid_rev'))

    def test_revision_exists_oserror(self):
        self.mock_run.side_effect = OSError('git not found')
        self.assertFalse(git_utils.revision_exists('any_rev'))


if __name__ == '__main__':
    unittest.main()

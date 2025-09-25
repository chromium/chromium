#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for workers."""

import pathlib
import subprocess
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import workers


class WorkDirUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the WorkDir class."""

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir('/tmp/src')
        self._setUpPatches()

    def _setUpPatches(self):
        """Set up patches for the tests."""
        rmtree_patcher = mock.patch('shutil.rmtree')
        self.mock_rmtree = rmtree_patcher.start()
        self.addCleanup(rmtree_patcher.stop)

        call_patcher = mock.patch('subprocess.call')
        self.mock_call = call_patcher.start()
        self.addCleanup(call_patcher.stop)

        check_call_patcher = mock.patch('subprocess.check_call')
        self.mock_check_call = check_call_patcher.start()
        self.addCleanup(check_call_patcher.stop)

    def test_enter_btrfs(self):
        """Tests that a btrfs snapshot is created when btrfs is true."""
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False,
                                  btrfs=True)
        with workdir as w:
            self.assertEqual(w, workdir)

        self.mock_check_call.assert_called_once_with(
            [
                'btrfs',
                'subvol',
                'snapshot',
                pathlib.Path('/tmp/src'),
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_enter_no_btrfs(self):
        """Tests that gclient-new-workdir is called when btrfs is false."""
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False,
                                  btrfs=False)
        with workdir as w:
            self.assertEqual(w, workdir)

        self.mock_check_call.assert_called_once_with(
            [
                'gclient-new-workdir.py',
                pathlib.Path('/tmp/src'),
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_enter_exists_no_force(self):
        """Tests that an error is raised if the workdir exists."""
        self.fs.create_dir('/tmp/workdir')
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False,
                                  btrfs=False)
        with self.assertRaises(FileExistsError):
            with workdir:
                pass

    def test_enter_exists_force(self):
        """Tests that the workdir is removed if it exists and force is on."""
        self.fs.create_dir('/tmp/workdir')
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=True,
                                  btrfs=True)
        with workdir:
            pass

        self.mock_call.assert_called_once_with(
            [
                'sudo',
                '-n',
                'btrfs',
                'subvolume',
                'delete',
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_exit_clean_btrfs(self):
        """Tests that the workdir is removed when clean is true w/ btrfs ."""
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=True,
                                  verbose=False,
                                  force=False,
                                  btrfs=True)
        with workdir:
            pass

        self.mock_call.assert_called_once_with(
            [
                'sudo',
                'btrfs',
                'subvolume',
                'delete',
                pathlib.Path('/tmp/workdir'),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )

    def test_exit_clean_no_btrfs(self):
        """Tests that the workdir is removed when clean is True w/o btrfs."""
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=True,
                                  verbose=False,
                                  force=False,
                                  btrfs=False)
        with workdir:
            pass

        self.mock_rmtree.assert_called_once_with(pathlib.Path('/tmp/workdir'))

    def test_exit_no_clean(self):
        """Tests that the workdir is not cleaned up when clean is False."""
        workdir = workers.WorkDir('workdir',
                                  pathlib.Path('/tmp/src'),
                                  clean=False,
                                  verbose=False,
                                  force=False,
                                  btrfs=False)
        with workdir:
            pass

        self.mock_call.assert_not_called()
        self.mock_rmtree.assert_not_called()


if __name__ == '__main__':
    unittest.main()

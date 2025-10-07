#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for checkout_helpers."""

import pathlib
import subprocess
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import checkout_helpers


class CheckBtrfsUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `check_btrfs` function."""

    def setUp(self):
        self.setUpPyfakefs()

    def tearDown(self):
        checkout_helpers.check_btrfs.cache_clear()

    @mock.patch('subprocess.run')
    def test_check_btrfs_is_btrfs(self, mock_run):
        """Tests that btrfs is detected correctly."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['stat', '-c', '%i', '/tmp'], returncode=0, stdout='256\n')
        with self.assertNoLogs():
            self.assertTrue(checkout_helpers.check_btrfs('/tmp'))

    @mock.patch('subprocess.run')
    def test_check_btrfs_is_not_btrfs(self, mock_run):
        """Tests that non-btrfs is detected correctly."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['stat', '-c', '%i', '/tmp'], returncode=0, stdout='123\n')
        with self.assertLogs(level='WARNING') as cm:
            self.assertFalse(checkout_helpers.check_btrfs('/tmp'))
            self.assertIn(
                'Warning: This is not running in a btrfs environment',
                cm.output[0])

    @mock.patch('subprocess.run')
    def test_check_btrfs_stat_fails(self, mock_run):
        """Tests that an exception is raised when stat fails."""
        mock_run.side_effect = subprocess.CalledProcessError(1, 'stat')
        with self.assertRaises(subprocess.CalledProcessError):
            checkout_helpers.check_btrfs('/tmp')

    @mock.patch('subprocess.run')
    def test_check_btrfs_is_cached(self, mock_run):
        """Tests that the result of check_btrfs is cached."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['stat', '-c', '%i', '/tmp'], returncode=0, stdout='256\n')
        checkout_helpers.check_btrfs('/tmp')
        checkout_helpers.check_btrfs('/tmp')
        mock_run.assert_called_once()




class GetGclientRootUnittest(unittest.TestCase):
    """Unit tests for the `get_gclient_root` function."""

    def tearDown(self):
        checkout_helpers.get_gclient_root.cache_clear()

    @mock.patch('subprocess.run')
    def test_get_gclient_root_success(self, mock_run):
        """Tests that the gclient root is returned on success."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['gclient', 'root'], returncode=0, stdout='/path/to/root\n')
        result = checkout_helpers.get_gclient_root()
        self.assertEqual(result, pathlib.Path('/path/to/root'))

    @mock.patch('subprocess.run')
    def test_get_gclient_root_failure(self, mock_run):
        """Tests that an exception is raised on failure."""
        mock_run.side_effect = subprocess.CalledProcessError(1, 'gclient root')
        with self.assertRaises(subprocess.CalledProcessError):
            checkout_helpers.get_gclient_root()

    @mock.patch('subprocess.run')
    def test_get_gclient_root_is_cached(self, mock_run):
        """Tests that the result of get_gclient_root is cached."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['gclient', 'root'], returncode=0, stdout='/path/to/root\n')
        checkout_helpers.get_gclient_root()
        checkout_helpers.get_gclient_root()
        mock_run.assert_called_once()


class GetDepotToolsPathUnittest(unittest.TestCase):
    """Unit tests for the `get_depot_tools_path` function."""

    def tearDown(self):
        checkout_helpers.get_depot_tools_path.cache_clear()

    @mock.patch('shutil.which')
    def test_get_depot_tools_path_success(self, mock_which):
        """Tests that the depot_tools path is returned on success."""
        mock_gclient_path = pathlib.Path('/path/to/depot_tools/gclient')
        mock_which.return_value = str(mock_gclient_path.resolve())
        result = checkout_helpers.get_depot_tools_path()
        self.assertEqual(result, mock_gclient_path.resolve().parent)

    @mock.patch('shutil.which')
    def test_get_depot_tools_path_failure(self, mock_which):
        """Tests that None is returned on failure."""
        mock_which.return_value = None
        result = checkout_helpers.get_depot_tools_path()
        self.assertIsNone(result)

    @mock.patch('shutil.which')
    def test_get_depot_tools_path_is_cached(self, mock_which):
        """Tests that the result of get_depot_tools_path is cached."""
        mock_which.return_value = str(
            pathlib.Path('/path/to/depot_tools/gclient'))
        checkout_helpers.get_depot_tools_path()
        checkout_helpers.get_depot_tools_path()
        mock_which.assert_called_once()


if __name__ == '__main__':
    unittest.main()

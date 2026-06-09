#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for cipd_helpers.py."""

import pathlib
import subprocess
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import cipd_helpers


class CipdHelpersTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()

        self.mock_run = mock.patch('subprocess.run').start()
        self.addCleanup(mock.patch.stopall)

    def test_initialize_cipd_root(self):
        with cipd_helpers.initialize_cipd_root() as root:
            self.assertTrue(self.fs.exists(str(root)))
            self.mock_run.assert_called_once_with(
                ['cipd', 'init', '-force', str(root)],
                check=True,
                capture_output=True,
                text=True,
            )
        self.assertFalse(self.fs.exists(str(root)))

    def test_install_package_success(self):
        cipd_root = pathlib.Path('/fake/tmp/dir')
        self.fs.create_dir(str(cipd_root))

        result = cipd_helpers.install_package('fake/package', 'latest',
                                              cipd_root)

        self.assertTrue(result)
        self.mock_run.assert_called_once_with(
            [
                'cipd',
                'install',
                'fake/package',
                'latest',
                '-root',
                str(cipd_root),
                '-log-level',
                'warning',
            ],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def test_install_package_failure(self):
        cipd_root = pathlib.Path('/fake/tmp/dir')
        self.fs.create_dir(str(cipd_root))
        self.mock_run.side_effect = subprocess.CalledProcessError(
            returncode=1, cmd='cipd install', output='Failed')

        result = cipd_helpers.install_package('fake/package', 'latest',
                                              cipd_root)

        self.assertFalse(result)
        self.mock_run.assert_called_once()


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for promptfoo_installation."""

import pathlib
import subprocess
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import promptfoo_installation

# pylint: disable=protected-access


class FromCipdPromptfooInstallationUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for FromCipdPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)
        check_call_patcher = mock.patch('subprocess.check_call')
        self.mock_check_call = check_call_patcher.start()
        self.addCleanup(check_call_patcher.stop)

    def test_setup(self):
        """Tests that setup runs the correct cipd commands."""
        promptfoo_installation.FromCipdPromptfooInstallation()

        self.mock_check_call.assert_has_calls([
            mock.call([
                'cipd', 'init', '-force',
                str(promptfoo_installation.CIPD_ROOT)
            ]),
            mock.call([
                'cipd', 'install', 'infra/3pp/tools/nodejs/linux-${arch}',
                'version:3@25.0.0', '-root', promptfoo_installation.CIPD_ROOT,
                '-log-level', 'warning'
            ],
                      stdout=subprocess.DEVNULL),
            mock.call([
                'cipd', 'install', 'infra/3pp/npm/promptfoo/linux-${arch}',
                'version:3@0.118.17', '-root',
                promptfoo_installation.CIPD_ROOT, '-log-level', 'warning'
            ],
                      stdout=subprocess.DEVNULL),
        ])

    def test_setup_already_installed(self):
        """Tests that setup does not init cipd if already installed."""
        executable_path = (promptfoo_installation.CIPD_ROOT / 'node_modules' /
                           '.bin' / 'promptfoo')
        self.fs.create_file(executable_path)
        promptfoo_installation.FromCipdPromptfooInstallation()

        self.mock_check_call.assert_has_calls([
            mock.call([
                'cipd', 'install', 'infra/3pp/tools/nodejs/linux-${arch}',
                'version:3@25.0.0', '-root', promptfoo_installation.CIPD_ROOT,
                '-log-level', 'warning'
            ],
                      stdout=subprocess.DEVNULL),
            mock.call([
                'cipd', 'install', 'infra/3pp/npm/promptfoo/linux-${arch}',
                'version:3@0.118.17', '-root',
                promptfoo_installation.CIPD_ROOT, '-log-level', 'warning'
            ],
                      stdout=subprocess.DEVNULL),
        ])
        # Check that 'cipd init' was NOT called.
        for call in self.mock_check_call.call_args_list:
            self.assertNotIn('init', call.args[0])

    def test_installed_true(self):
        """Tests that installed is true when the executable exists."""
        executable_path = (promptfoo_installation.CIPD_ROOT / 'node_modules' /
                           '.bin' / 'promptfoo')
        self.fs.create_file(executable_path)
        installation = promptfoo_installation.FromCipdPromptfooInstallation()
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when the executable does not exist."""
        installation = promptfoo_installation.FromCipdPromptfooInstallation()
        self.assertFalse(installation.installed)

    def test_run(self):
        """Tests that run calls the promptfoo executable."""
        executable_path = (promptfoo_installation.CIPD_ROOT / 'node_modules' /
                           '.bin' / 'promptfoo')
        self.fs.create_file(executable_path)
        node_path = promptfoo_installation.CIPD_ROOT / 'bin' / 'node'
        self.fs.create_file(node_path)
        installation = promptfoo_installation.FromCipdPromptfooInstallation()
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        self.mock_run.assert_called_once_with([
            str(node_path),
            str(executable_path), 'eval', '-c', 'config.yaml'
        ],
                                              cwd='/tmp/test',
                                              check=False,
                                              text=True,
                                              stdout=subprocess.PIPE,
                                              stderr=subprocess.STDOUT)


class PreinstalledPromptfooInstallationUnittest(
        fake_filesystem_unittest.TestCase):
    """Unit tests for PreinstalledPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def test_installed_true(self):
        """Tests that installed is true when the executable exists."""
        self.fs.create_file('/tmp/promptfoo')
        installation = (
            promptfoo_installation.PreinstalledPromptfooInstallation(
                pathlib.Path('/tmp/promptfoo')))
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when the executable does not exist."""
        installation = (
            promptfoo_installation.PreinstalledPromptfooInstallation(
                pathlib.Path('/tmp/promptfoo')))
        self.assertFalse(installation.installed)

    def test_run(self):
        """Tests that run calls the promptfoo executable."""
        self.fs.create_file('/tmp/promptfoo')
        installation = (
            promptfoo_installation.PreinstalledPromptfooInstallation(
                pathlib.Path('/tmp/promptfoo')))
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        self.mock_run.assert_called_once_with(
            [str(pathlib.Path('/tmp/promptfoo')), 'eval', '-c', 'config.yaml'],
            cwd='/tmp/test',
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)


if __name__ == '__main__':
    unittest.main()

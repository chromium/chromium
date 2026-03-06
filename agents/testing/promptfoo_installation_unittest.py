#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for promptfoo_installation."""

import pathlib
import subprocess
import signal
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import promptfoo_installation

# pylint: disable=protected-access


class FromCipdPromptfooInstallationUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for FromCipdPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('promptfoo_installation._run')
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
                'cipd',
                'init',
                '-force',
                str(promptfoo_installation.CIPD_ROOT),
            ]),
            mock.call([
                'cipd',
                'install',
                'infra/3pp/tools/nodejs/linux-${arch}',
                mock.ANY,
                '-root',
                promptfoo_installation.CIPD_ROOT,
                '-log-level',
                'warning',
            ],
                      stdout=subprocess.DEVNULL),
            mock.call([
                'cipd',
                'install',
                'infra/3pp/npm/promptfoo/linux-${arch}',
                mock.ANY,
                '-root',
                promptfoo_installation.CIPD_ROOT,
                '-log-level',
                'warning',
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
                'cipd',
                'install',
                'infra/3pp/tools/nodejs/linux-${arch}',
                mock.ANY,
                '-root',
                promptfoo_installation.CIPD_ROOT,
                '-log-level',
                'warning',
            ],
                      stdout=subprocess.DEVNULL),
            mock.call([
                'cipd',
                'install',
                'infra/3pp/npm/promptfoo/linux-${arch}',
                mock.ANY,
                '-root',
                promptfoo_installation.CIPD_ROOT,
                '-log-level',
                'warning',
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
                                              cwd='/tmp/test')


class PreinstalledPromptfooInstallationUnittest(
        fake_filesystem_unittest.TestCase):
    """Unit tests for PreinstalledPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('promptfoo_installation._run')
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
            cwd='/tmp/test')


class RunUnittest(unittest.TestCase):
    """Unit tests for _run."""

    @mock.patch('sys.platform', 'win32')
    @mock.patch('subprocess.run')
    def test_win32(self, mock_run):
        mock_run.return_value = subprocess.CompletedProcess(['cmd'], 0, 'out',
                                                            '')
        result = promptfoo_installation._run(['cmd'], cwd='/tmp')
        mock_run.assert_called_once_with(['cmd'],
                                         cwd='/tmp',
                                         check=False,
                                         text=True,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, 'out')

    @mock.patch('signal.SIGKILL', 9, create=True)
    @mock.patch('sys.platform', 'linux')
    @mock.patch('os.killpg', create=True)
    @mock.patch('subprocess.Popen')
    def test_unix(self, mock_popen, mock_killpg):
        mock_proc = mock.MagicMock()
        mock_proc.pid = 12345
        mock_proc.args = ['cmd']
        mock_proc.returncode = 0
        mock_proc.communicate.return_value = ('out', '')
        mock_proc.__enter__.return_value = mock_proc
        mock_popen.return_value = mock_proc

        result = promptfoo_installation._run(['cmd'], cwd='/tmp')

        mock_popen.assert_called_once_with(['cmd'],
                                           cwd='/tmp',
                                           text=True,
                                           stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT,
                                           start_new_session=True)
        mock_proc.communicate.assert_called_once()
        mock_killpg.assert_called_once_with(12345, signal.SIGKILL)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, 'out')

    @mock.patch('signal.SIGKILL', 9, create=True)
    @mock.patch('sys.platform', 'linux')
    @mock.patch('os.killpg', create=True)
    @mock.patch('subprocess.Popen')
    def test_unix_killpg_oserror(self, mock_popen, mock_killpg):
        mock_proc = mock.MagicMock()
        mock_proc.pid = 12345
        mock_proc.args = ['cmd']
        mock_proc.returncode = 0
        mock_proc.communicate.return_value = ('out', '')
        mock_proc.__enter__.return_value = mock_proc
        mock_popen.return_value = mock_proc
        mock_killpg.side_effect = OSError('Process not found')

        result = promptfoo_installation._run(['cmd'], cwd='/tmp')

        mock_killpg.assert_called_once_with(12345, signal.SIGKILL)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, 'out')


if __name__ == '__main__':
    unittest.main()

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


class FromNpmPromptfooInstallationUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for FromNpmPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def test_setup(self):
        """Tests that setup runs the correct npm commands."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), '0.42.0')
        installation.setup()

        self.mock_run.assert_has_calls([
            mock.call(['npm', 'init', '-y'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
            mock.call(['npm', 'install', 'promptfoo@0.42.0'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
        ])

    def test_setup_no_version(self):
        """Tests that setup uses latest when no version is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), None)
        installation.setup()

        self.mock_run.assert_has_calls([
            mock.call(['npm', 'init', '-y'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
            mock.call(['npm', 'install', 'promptfoo@latest'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
        ])


    def test_installed_true(self):
        """Tests that installed is true when the executable exists."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when the executable does not exist."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        self.assertFalse(installation.installed)

    def test_run(self):
        """Tests that run calls the promptfoo executable."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        executable = '/tmp/promptfoo/node_modules/.bin/promptfoo'
        self.mock_run.assert_called_once_with(
            [str(pathlib.Path(executable)), 'eval', '-c', 'config.yaml'],
            cwd='/tmp/test',
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    def test_run_no_cwd(self):
        """Tests that run defaults cwd to None."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        installation.run(['eval', '-c', 'config.yaml'])
        executable = '/tmp/promptfoo/node_modules/.bin/promptfoo'
        self.mock_run.assert_called_once_with(
            [str(pathlib.Path(executable)), 'eval', '-c', 'config.yaml'],
            cwd=None,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    def test_run_failure(self):
        """Tests that run returns the CompletedProcess on failure."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        self.mock_run.return_value = subprocess.CompletedProcess(
            args=['/tmp/promptfoo/node_modules/.bin/promptfoo', 'eval'],
            returncode=1,
            stdout='error')
        result = installation.run(['eval'])
        self.assertEqual(result, self.mock_run.return_value)

    def test_cleanup(self):
        """Tests that cleanup removes the installation directory."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        installation.cleanup()
        self.assertFalse(pathlib.Path('/tmp/promptfoo').exists())

    def test_cleanup_not_exists(self):
        """Tests that cleanup does not error if the directory is gone."""
        installation = promptfoo_installation.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo_nonexistent'), 'latest')
        installation.cleanup()



class FromSourcePromptfooInstallationUnittest(fake_filesystem_unittest.TestCase
                                              ):
    """Unit tests for FromSourcePromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()
        run_patcher = mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def test_setup(self):
        """Tests that setup runs the correct git and npm commands."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'my-rev')
        installation.setup()

        self.mock_run.assert_has_calls([
            mock.call([
                'git', 'clone', 'https://github.com/promptfoo/promptfoo',
                pathlib.Path('/tmp/promptfoo')
            ],
                      check=True),
            mock.call(['git', 'checkout', 'my-rev'],
                      check=True,
                      cwd=pathlib.Path('/tmp/promptfoo')),
            mock.call(['npm', 'install'],
                      check=True,
                      cwd=pathlib.Path('/tmp/promptfoo')),
            mock.call(['npm', 'run', 'build'],
                      check=True,
                      cwd=pathlib.Path('/tmp/promptfoo')),
        ])

    def test_setup_no_revision(self):
        """Tests that setup does not checkout when no revision is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), None)
        installation.setup()

        self.mock_run.assert_has_calls([
            mock.call([
                'git', 'clone', 'https://github.com/promptfoo/promptfoo',
                pathlib.Path('/tmp/promptfoo')
            ],
                      check=True),
            mock.call(['npm', 'install'],
                      check=True,
                      cwd=pathlib.Path('/tmp/promptfoo')),
            mock.call(['npm', 'run', 'build'],
                      check=True,
                      cwd=pathlib.Path('/tmp/promptfoo')),
        ])
        for call in self.mock_run.call_args_list:
            self.assertNotIn('checkout', call.args[0])

    def test_installed_true(self):
        """Tests that installed is true when .git directory exists."""
        self.fs.create_dir('/tmp/promptfoo/.git')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when .git directory does not exist."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        self.assertFalse(installation.installed)

    def test_run(self):
        """Tests that run calls node with the correct script."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        main_js = '/tmp/promptfoo/dist/src/main.js'
        self.mock_run.assert_called_once_with(
            [str(pathlib.Path(main_js)), 'eval', '-c', 'config.yaml'],
            cwd='/tmp/test',
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    def test_run_no_cwd(self):
        """Tests that run defaults cwd to None."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        installation.run(['eval', '-c', 'config.yaml'])
        main_js = '/tmp/promptfoo/dist/src/main.js'
        self.mock_run.assert_called_once_with(
            [str(pathlib.Path(main_js)), 'eval', '-c', 'config.yaml'],
            cwd=None,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    def test_run_failure(self):
        """Tests that run returns the CompletedProcess on failure."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        self.mock_run.return_value = subprocess.CompletedProcess(
            args=['/tmp/promptfoo/dist/src/main.js', 'eval'],
            returncode=1,
            stdout='error')
        result = installation.run(['eval'])
        self.assertEqual(result, self.mock_run.return_value)

    def test_cleanup(self):
        """Tests that cleanup removes the installation directory."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        installation.cleanup()
        self.assertFalse(pathlib.Path('/tmp/promptfoo').exists())

    def test_cleanup_not_exists(self):
        """Tests that cleanup does not error if the directory is gone."""
        installation = promptfoo_installation.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo_nonexistent'), 'main')
        installation.cleanup()



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

    def test_cleanup(self):
        """Tests that cleanup is a no-op."""
        self.fs.create_file('/tmp/promptfoo')
        installation = (
            promptfoo_installation.PreinstalledPromptfooInstallation(
                pathlib.Path('/tmp/promptfoo')))
        installation.cleanup()
        self.assertTrue(pathlib.Path('/tmp/promptfoo').exists())


class SetupPromptfooUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for setup_promptfoo."""

    def setUp(self):
        self.setUpPyfakefs()
        npm_patcher = mock.patch(
            'promptfoo_installation.FromNpmPromptfooInstallation')
        self.mock_npm_install = npm_patcher.start()
        self.addCleanup(npm_patcher.stop)

        src_patcher = mock.patch(
            'promptfoo_installation.FromSourcePromptfooInstallation')
        self.mock_src_install = src_patcher.start()
        self.addCleanup(src_patcher.stop)

    def test_use_npm_with_version(self):
        """Tests that npm is used when a version is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_npm_instance = self.mock_npm_install.return_value
        promptfoo_installation.setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               None, '0.42.0')
        self.mock_npm_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), '0.42.0')
        self.mock_src_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), None)
        mock_npm_instance.cleanup.assert_called_once()
        mock_npm_instance.setup.assert_called_once()

    def test_use_src_with_revision(self):
        """Tests that source is used when a revision is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = self.mock_src_install.return_value
        promptfoo_installation.setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               'my-rev', None)
        self.mock_npm_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), None)
        self.mock_src_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), 'my-rev')
        mock_src_instance.cleanup.assert_called_once()
        mock_src_instance.setup.assert_called_once()

    def test_no_args_detect_existing_src(self):
        """Tests that an existing source installation is detected."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = self.mock_src_install.return_value
        mock_src_instance.installed = True
        mock_npm_instance = self.mock_npm_install.return_value
        mock_npm_instance.installed = False

        result = promptfoo_installation.setup_promptfoo(
            pathlib.Path('/tmp/promptfoo'), None, None)

        self.assertEqual(result, mock_src_instance)
        mock_src_instance.cleanup.assert_not_called()
        mock_src_instance.setup.assert_not_called()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()

    def test_no_args_detect_existing_npm(self):
        """Tests that an existing npm installation is detected."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = self.mock_src_install.return_value
        mock_src_instance.installed = False
        mock_npm_instance = self.mock_npm_install.return_value
        mock_npm_instance.installed = True

        result = promptfoo_installation.setup_promptfoo(
            pathlib.Path('/tmp/promptfoo'), None, None)

        self.assertEqual(result, mock_npm_instance)
        mock_src_instance.cleanup.assert_not_called()
        mock_src_instance.setup.assert_not_called()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()

    def test_no_args_no_existing_installs(self):
        """Tests that source is used when no installation is found."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = self.mock_src_install.return_value
        mock_src_instance.installed = False

        def setup_effect():
            mock_src_instance.installed = True

        mock_src_instance.setup.side_effect = setup_effect
        mock_npm_instance = self.mock_npm_install.return_value
        mock_npm_instance.installed = False

        result = promptfoo_installation.setup_promptfoo(
            pathlib.Path('/tmp/promptfoo'), None, None)

        self.assertEqual(result, mock_src_instance)
        mock_src_instance.cleanup.assert_called_once()
        mock_src_instance.setup.assert_called_once()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()

    def test_use_npm_with_version_and_revision(self):
        """Tests that npm is used when a version and revision are provided."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_npm_instance = self.mock_npm_install.return_value
        mock_src_instance = self.mock_src_install.return_value
        promptfoo_installation.setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               'my-rev', '0.42.0')
        self.mock_npm_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), '0.42.0')
        self.mock_src_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), 'my-rev')
        mock_npm_instance.cleanup.assert_called_once()
        mock_npm_instance.setup.assert_called_once()
        mock_src_instance.cleanup.assert_not_called()
        mock_src_instance.setup.assert_not_called()


if __name__ == '__main__':
    unittest.main()

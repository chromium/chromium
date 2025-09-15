#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for eval_prompts."""

import os
import pathlib
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import eval_prompts

# pylint: disable=protected-access


class FromNpmPromptfooInstallationUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for FromNpmPromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('subprocess.run')
    def test_setup(self, mock_run):
        """Tests that setup runs the correct npm commands."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), '0.42.0')
        installation.setup()

        mock_run.assert_has_calls([
            mock.call(['npm', 'init', '-y'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
            mock.call(['npm', 'install', 'promptfoo@0.42.0'],
                      cwd=pathlib.Path('/tmp/promptfoo'),
                      check=True),
        ])

    def test_installed_true(self):
        """Tests that installed is true when the executable exists."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = eval_prompts.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when the executable does not exist."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        self.assertFalse(installation.installed)

    @mock.patch('subprocess.run')
    def test_run(self, mock_run):
        """Tests that run calls the promptfoo executable."""
        self.fs.create_file('/tmp/promptfoo/node_modules/.bin/promptfoo')
        installation = eval_prompts.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        executable = '/tmp/promptfoo/node_modules/.bin/promptfoo'
        mock_run.assert_called_once_with(
            [str(pathlib.Path(executable)), 'eval', '-c', 'config.yaml'],
            cwd='/tmp/test',
            check=False)

    def test_cleanup(self):
        """Tests that cleanup removes the installation directory."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromNpmPromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'latest')
        installation.cleanup()
        self.assertFalse(pathlib.Path('/tmp/promptfoo').exists())


class FromSourcePromptfooInstallationUnittest(fake_filesystem_unittest.TestCase
                                              ):
    """Unit tests for FromSourcePromptfooInstallation."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('subprocess.run')
    def test_setup(self, mock_run):
        """Tests that setup runs the correct git and npm commands."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'my-rev')
        installation.setup()

        mock_run.assert_has_calls([
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

    def test_installed_true(self):
        """Tests that installed is true when .git directory exists."""
        self.fs.create_dir('/tmp/promptfoo/.git')
        installation = eval_prompts.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        self.assertTrue(installation.installed)

    def test_installed_false(self):
        """Tests that installed is false when .git directory does not exist."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        self.assertFalse(installation.installed)

    @mock.patch('subprocess.run')
    def test_run(self, mock_run):
        """Tests that run calls node with the correct script."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        installation.run(['eval', '-c', 'config.yaml'], cwd='/tmp/test')
        main_js = '/tmp/promptfoo/dist/src/main.js'
        mock_run.assert_called_once_with(
            [str(pathlib.Path(main_js)), 'eval', '-c', 'config.yaml'],
            cwd='/tmp/test',
            check=False)

    def test_cleanup(self):
        """Tests that cleanup removes the installation directory."""
        self.fs.create_dir('/tmp/promptfoo')
        installation = eval_prompts.FromSourcePromptfooInstallation(
            pathlib.Path('/tmp/promptfoo'), 'main')
        installation.cleanup()
        self.assertFalse(pathlib.Path('/tmp/promptfoo').exists())


class SetupPromptfooUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for _setup_promptfoo."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('eval_prompts.FromNpmPromptfooInstallation')
    @mock.patch('eval_prompts.FromSourcePromptfooInstallation')
    def test_use_npm_with_version(self, mock_src_install, mock_npm_install):
        """Tests that npm is used when a version is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_npm_instance = mock_npm_install.return_value
        eval_prompts._setup_promptfoo(pathlib.Path('/tmp/promptfoo'), None,
                                      '0.42.0')
        mock_npm_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), '0.42.0')
        mock_src_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), None)
        mock_npm_instance.cleanup.assert_called_once()
        mock_npm_instance.setup.assert_called_once()

    @mock.patch('eval_prompts.FromNpmPromptfooInstallation')
    @mock.patch('eval_prompts.FromSourcePromptfooInstallation')
    def test_use_src_with_revision(self, mock_src_install, mock_npm_install):
        """Tests that source is used when a revision is provided."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = mock_src_install.return_value
        eval_prompts._setup_promptfoo(pathlib.Path('/tmp/promptfoo'), 'my-rev',
                                      None)
        mock_npm_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), None)
        mock_src_install.assert_called_once_with(
            pathlib.Path('/tmp/promptfoo'), 'my-rev')
        mock_src_instance.cleanup.assert_called_once()
        mock_src_instance.setup.assert_called_once()

    @mock.patch('eval_prompts.FromNpmPromptfooInstallation')
    @mock.patch('eval_prompts.FromSourcePromptfooInstallation')
    def test_no_args_detect_existing_src(self, mock_src_install,
                                         mock_npm_install):
        """Tests that an existing source installation is detected."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = mock_src_install.return_value
        mock_src_instance.installed = True
        mock_npm_instance = mock_npm_install.return_value
        mock_npm_instance.installed = False

        result = eval_prompts._setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               None, None)

        self.assertEqual(result, mock_src_instance)
        mock_src_instance.cleanup.assert_not_called()
        mock_src_instance.setup.assert_not_called()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()

    @mock.patch('eval_prompts.FromNpmPromptfooInstallation')
    @mock.patch('eval_prompts.FromSourcePromptfooInstallation')
    def test_no_args_detect_existing_npm(self, mock_src_install,
                                         mock_npm_install):
        """Tests that an existing npm installation is detected."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = mock_src_install.return_value
        mock_src_instance.installed = False
        mock_npm_instance = mock_npm_install.return_value
        mock_npm_instance.installed = True

        result = eval_prompts._setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               None, None)

        self.assertEqual(result, mock_npm_instance)
        mock_src_instance.cleanup.assert_not_called()
        mock_src_instance.setup.assert_not_called()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()

    @mock.patch('eval_prompts.FromNpmPromptfooInstallation')
    @mock.patch('eval_prompts.FromSourcePromptfooInstallation')
    def test_no_args_no_existing_installs(self, mock_src_install,
                                          mock_npm_install):
        """Tests that source is used when no installation is found."""
        self.fs.create_dir('/tmp/promptfoo')
        mock_src_instance = mock_src_install.return_value
        mock_src_instance.installed = False

        def setup_effect():
            mock_src_instance.installed = True

        mock_src_instance.setup.side_effect = setup_effect
        mock_npm_instance = mock_npm_install.return_value
        mock_npm_instance.installed = False

        result = eval_prompts._setup_promptfoo(pathlib.Path('/tmp/promptfoo'),
                                               None, None)

        self.assertEqual(result, mock_src_instance)
        mock_src_instance.cleanup.assert_called_once()
        mock_src_instance.setup.assert_called_once()
        mock_npm_instance.cleanup.assert_not_called()
        mock_npm_instance.setup.assert_not_called()


class DetermineShardValuesUnittest(unittest.TestCase):
    """Unit tests for the `_determine_shard_values` function."""

    @mock.patch.dict(os.environ, {}, clear=True)
    def test_no_args_no_env(self):
        """Tests that the default values are returned w/o shard info."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (0, 1))

    def test_args_provided(self):
        """Tests that the argument values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(1, 3), (1, 3))
        self.assertEqual(eval_prompts._determine_shard_values(0, 1), (0, 1))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_env_vars_provided(self):
        """Tests that the env variable values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (2, 4))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_args_and_env_vars_provided(self):
        """Tests that arg values take precedence over environment variables."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(1, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    def test_shard_index_arg_only(self):
        """Tests that ValueError is raised if only shard_index is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(1, None)

    def test_total_shards_arg_only(self):
        """Tests that ValueError is raised if only total_shards is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, 3)

    @mock.patch.dict(os.environ, {eval_prompts._SHARD_INDEX_ENV_VAR: '1'},
                     clear=True)
    def test_shard_index_env_only(self):
        """Tests that a ValueError is raised if only shard_index is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    @mock.patch.dict(os.environ, {eval_prompts._TOTAL_SHARDS_ENV_VAR: '3'},
                     clear=True)
    def test_total_shards_env_only(self):
        """Tests that a ValueError is raised if only total_shards is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    def test_negative_shard_index(self):
        """Tests that a ValueError is raised for a negative shard_index."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be non-negative'):
            eval_prompts._determine_shard_values(-1, 3)

    def test_zero_total_shards(self):
        """Tests that a ValueError is raised for a total_shards of zero."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, 0)

    def test_negative_total_shards(self):
        """Tests that a ValueError is raised for a negative total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, -1)

    def test_shard_index_equal_to_total_shards(self):
        """Tests that a ValueError is raised if shard_index == total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(3, 3)

    def test_shard_index_greater_than_total_shards(self):
        """Tests that a ValueError is raised if shard_index > total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(4, 3)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_total_shards_from_args_shard_index_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(None, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_shard_index_from_args_total_shards_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(2, None),
                             (2, 5))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for install.py."""

import io
from pathlib import Path
import unittest
import unittest.mock

import install
from pyfakefs import fake_filesystem_unittest

# pylint: disable=protected-access


class InstallTest(fake_filesystem_unittest.TestCase):
    """Tests for the extension installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.setUpPyfakefs(additional_skip_names=['subprocess'])
        self.tmpdir = '/tmp/test'
        self.project_root = Path(self.tmpdir) / 'src'
        self.fs.create_dir(self.project_root)

        self.source_extensions_dir = self.project_root / 'agents' / 'extensions'
        self.fs.create_dir(self.source_extensions_dir)
        self.install_script_path = self.source_extensions_dir / 'install.py'
        self.fs.create_file(self.install_script_path)

        self.testing_extensions_dir = (self.project_root / 'agents' /
                                       'testing' / 'extensions')
        self.fs.create_dir(self.testing_extensions_dir)

        self.internal_extensions_dir = (self.project_root / 'internal' /
                                        'agents' / 'extensions')
        self.fs.create_dir(self.internal_extensions_dir)

        # Create sample extensions
        self.extension1_dir = self.source_extensions_dir / 'sample_1'
        self.fs.create_dir(self.extension1_dir)
        self.fs.create_file(
            self.extension1_dir / 'gemini-extension.json',
            contents='{"name": "sample_1", "version": "1.0.0"}',
        )

        self.test_extension_dir = self.testing_extensions_dir / 'test_sample'
        self.fs.create_dir(self.test_extension_dir)
        self.fs.create_file(
            self.test_extension_dir / 'gemini-extension.json',
            contents='{"name": "test_sample", "version": "1.0.0"}',
        )

        self.mock_run_command_patcher = unittest.mock.patch(
            'install._run_command')
        self.mock_run_command = self.mock_run_command_patcher.start()
        self.addCleanup(self.mock_run_command_patcher.stop)

        self.mock_check_version = unittest.mock.patch(
            'install.check_gemini_version')
        self.mock_check_version.start()
        self.addCleanup(self.mock_check_version.stop)

        self.mock_get_project_root_patcher = unittest.mock.patch(
            'install.get_project_root')
        self.mock_get_project_root = self.mock_get_project_root_patcher.start()
        self.addCleanup(self.mock_get_project_root_patcher.stop)
        self.mock_get_project_root.return_value = self.project_root

        self.mock_subprocess_run_patcher = unittest.mock.patch(
            'subprocess.run')
        self.mock_subprocess_run = self.mock_subprocess_run_patcher.start()
        self.addCleanup(self.mock_subprocess_run_patcher.stop)

    def test_find_extensions_dir_for_extension(self):
        """Tests finding an extension directory."""
        extensions_dirs = install.get_extensions_dirs(self.project_root)
        # Extension in source directory
        ext_dir = install.find_extensions_dir_for_extension(
            'sample_1', extensions_dirs)
        self.assertEqual(ext_dir, self.source_extensions_dir)

        # Extension in internal directory
        internal_extension_dir = self.internal_extensions_dir / 'internal_ext'
        self.fs.create_dir(internal_extension_dir)
        self.fs.create_file(
            internal_extension_dir / 'gemini-extension.json',
            contents='{"name": "internal_ext", "version": "1.0.0"}',
        )
        extensions_dirs = install.get_extensions_dirs(self.project_root)
        ext_dir = install.find_extensions_dir_for_extension(
            'internal_ext', extensions_dirs)
        self.assertEqual(ext_dir, self.internal_extensions_dir)

        # Extension in testing directory
        extensions_dirs = install.get_extensions_dirs(
            self.project_root,
            extra_extensions_dirs=[self.testing_extensions_dir])
        ext_dir = install.find_extensions_dir_for_extension(
            'test_sample', extensions_dirs)
        self.assertEqual(ext_dir, self.testing_extensions_dir)

    def test_get_extensions_dirs(self):
        """Tests that get_extensions_dirs returns correct directories."""
        # By default, test extensions should not be included
        dirs = install.get_extensions_dirs(self.project_root)
        self.assertIn(self.source_extensions_dir, dirs)
        self.assertIn(self.internal_extensions_dir, dirs)
        self.assertNotIn(self.testing_extensions_dir, dirs)

        dirs = install.get_extensions_dirs(
            self.project_root,
            extra_extensions_dirs=[self.testing_extensions_dir])
        self.assertIn(self.source_extensions_dir, dirs)
        self.assertIn(self.internal_extensions_dir, dirs)
        self.assertIn(self.testing_extensions_dir, dirs)

    def test_get_extensions_dirs_no_project_root(self):
        """Tests get_extensions_dirs() when no project root is found."""
        extensions_dirs = install.get_extensions_dirs(None)
        self.assertEqual(extensions_dirs, [])

    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_copy(self, mock_find_dir):
        """Tests add command with copy."""
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', '--copy', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with([
            'gemini', 'extensions', 'install',
            str(self.source_extensions_dir / 'sample_1')
        ],
                                                      skip_prompt=False)

    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_link(self, mock_find_dir):
        """Tests add command."""
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            [
                'gemini', 'extensions', 'link',
                str(self.source_extensions_dir / 'sample_1')
            ],
            skip_prompt=False)

    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_skip_prompt(self, mock_find_dir):
        """Tests that the skip_prompt flag is accepted."""
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch(
                'sys.argv',
            ['install.py', 'add', '--skip-prompt', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            [
                'gemini', 'extensions', 'link',
                str(self.source_extensions_dir / 'sample_1')
            ],
            skip_prompt=True)

    def test_add_test_extension(self):
        """Tests add command with a test extension."""
        with unittest.mock.patch('sys.argv', [
                'install.py', '--extra-extensions-dir',
                str(self.testing_extensions_dir), 'add', 'test_sample'
        ]):
            install.main()
        self.mock_run_command.assert_called_once_with(
            [
                'gemini', 'extensions', 'link',
                str(self.testing_extensions_dir / 'test_sample')
            ],
            skip_prompt=False)

    def test_add_test_extension_without_flag_fails(self):
        """Tests add command with a test extension."""
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', 'test_sample']):
            with self.assertRaises(SystemExit):
                install.main()


    def test_add_invalid_extension(self):
        """Tests add command with an invalid extension."""
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', 'nonexistent']):
            with unittest.mock.patch('sys.stderr',
                                     new_callable=io.StringIO) as mock_stderr:
                with self.assertRaises(SystemExit) as e:
                    install.main()
                self.assertEqual(e.exception.code, 1)
                self.assertIn("Extension 'nonexistent' not found.",
                              mock_stderr.getvalue())
        self.mock_run_command.assert_not_called()

    def test_update_extension(self):
        """Tests update command."""
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'update', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'update', 'sample_1'], skip_prompt=False)

    def test_update_all_extensions(self):
        """Tests update command with no extension specified."""
        with unittest.mock.patch('sys.argv', ['install.py', 'update']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'update', '--all'])

    def test_remove_extension(self):
        """Tests remove command."""
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'remove', 'sample-1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'uninstall', 'sample-1'])

    @unittest.mock.patch('pathlib.Path.home')
    def test_remove_legacy_extension(self, mock_home):
        """Tests remove command for legacy extensions with underscores."""
        fake_home = Path(self.tmpdir) / 'home'
        mock_home.return_value = fake_home

        # Set up a legacy extension
        legacy_extension_dir = (install.get_global_extension_dir() /
                                'my_legacy_ext')
        self.fs.create_dir(legacy_extension_dir)
        self.assertTrue(legacy_extension_dir.exists())

        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'remove', 'my_legacy_ext']):
            install.main()

        self.mock_run_command.assert_not_called()
        self.assertFalse(legacy_extension_dir.exists())

    def test_list_extensions(self):
        """Tests the list command, showing all extensions."""
        self.mock_subprocess_run.return_value.stdout = """
✓ user-enabled (1.0.0)
 ID: abc
 Path: /path/to/user-enabled
 Source: /path/to/source/user-enabled (Type: link)
 Enabled (User): true
 Enabled (Workspace): false

✓ workspace-enabled (2.0.0)
 ID: def
 Path: /path/to/workspace-enabled
 Source: /path/to/source/workspace-enabled (Type: local)
 Enabled (User): false
 Enabled (Workspace): true

✓ both-enabled (3.0.0)
 ID: ghi
 Path: /path/to/both-enabled
 Source: /path/to/source/both-enabled (Type: local)
 Enabled (User): true
 Enabled (Workspace): true
        """
        self.mock_subprocess_run.return_value.returncode = 0

        with unittest.mock.patch('sys.argv', ['install.py', 'list']):
            with unittest.mock.patch('sys.stdout',
                                     new_callable=io.StringIO) as mock_stdout:
                install.main()
                output = mock_stdout.getvalue()

        expected_extensions = {
            'workspace-enabled':
            install.ExtensionInfo(name='workspace-enabled',
                                  installed='2.0.0',
                                  linked=False,
                                  enabled_for_workspace=True),
            'user-enabled':
            install.ExtensionInfo(name='user-enabled',
                                  installed='1.0.0',
                                  linked=True,
                                  enabled_for_workspace=False),
            'both-enabled':
            install.ExtensionInfo(name='both-enabled',
                                  installed='3.0.0',
                                  linked=False,
                                  enabled_for_workspace=True),
            'sample_1':
            install.ExtensionInfo(name='sample_1', available='1.0.0'),
        }
        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as expected_stdout:
            install._print_extensions_table(expected_extensions)
            expected_output = expected_stdout.getvalue()

        self.assertEqual(output, expected_output)

    def test_list_extensions_no_installed(self):
        """Tests the list command with no installed extensions."""
        self.mock_subprocess_run.return_value.stdout = ''
        self.mock_subprocess_run.return_value.returncode = 0

        with unittest.mock.patch('sys.argv', ['install.py', 'list']):
            with unittest.mock.patch('sys.stdout',
                                     new_callable=io.StringIO) as mock_stdout:
                install.main()
                output = mock_stdout.getvalue()

        expected_extensions = {
            'sample_1': install.ExtensionInfo(name='sample_1',
                                              available='1.0.0'),
        }
        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as expected_stdout:
            install._print_extensions_table(expected_extensions)
            expected_output = expected_stdout.getvalue()

        self.assertEqual(output, expected_output)

    def test_list_extensions_empty_table(self):
        """Tests the list command with no available or installed extensions."""
        self.mock_subprocess_run.return_value.stdout = ''
        self.mock_subprocess_run.return_value.returncode = 0

        # Remove the sample extension created in setUp
        self.fs.remove_object(str(self.extension1_dir))

        with unittest.mock.patch('sys.argv', ['install.py', 'list']):
            with unittest.mock.patch('sys.stdout',
                                     new_callable=io.StringIO) as mock_stdout:
                install.main()
                output = mock_stdout.getvalue()
                expected_output = (
                    'EXTENSION  AVAILABLE  INSTALLED  LINKED  ENABLED\n'
                    '---------  ---------  ---------  ------  -------\n')
                self.assertEqual(output, expected_output)

    def test_print_extensions_table_formatting(self):
        """Tests the formatting of the extensions table."""
        extensions_data = {
            'ext_a':
            install.ExtensionInfo(name='ext_a',
                                  available='1.0.0',
                                  installed='1.0.0',
                                  linked=True,
                                  enabled_for_workspace=True),
            'another_extension':
            install.ExtensionInfo(name='another_extension',
                                  available='2.0.0',
                                  installed='-',
                                  linked=False,
                                  enabled_for_workspace=False),
            'third_ext':
            install.ExtensionInfo(name='third_ext',
                                  available='-',
                                  installed='3.0.0',
                                  linked=False,
                                  enabled_for_workspace=True),
        }
        expected_output = (
            'EXTENSION          AVAILABLE  INSTALLED  LINKED  ENABLED  \n'
            '-----------------  ---------  ---------  ------  ---------\n'
            'another_extension  2.0.0      -          no      -        \n'
            'ext_a              1.0.0      1.0.0      yes     workspace\n'
            'third_ext          -          3.0.0      no      workspace\n')
        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            install._print_extensions_table(extensions_data)
            self.assertEqual(mock_stdout.getvalue(), expected_output)

    def test_find_extensions_dir_for_nonexistent_extension(self):
        """Tests finding a non-existent extension."""
        extensions_dirs = install.get_extensions_dirs(self.project_root)
        ext_dir = install.find_extensions_dir_for_extension(
            'nonexistent', extensions_dirs)
        self.assertIsNone(ext_dir)

    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_fix_extensions(self, mock_find_dir):
        """Tests fix command."""
        mock_find_dir.return_value = self.source_extensions_dir
        project_extensions_dir = self.project_root / '.gemini' / 'extensions'
        self.fs.create_dir(project_extensions_dir)
        self.fs.create_file(
            project_extensions_dir / 'sample_1' / 'gemini-extension.json',
            contents='{"name": "sample_1", "version": "1.0.0"}',
        )

        with unittest.mock.patch('sys.argv', ['install.py', 'fix']):
            install.main()

        calls = [
            unittest.mock.call([
                'gemini', 'extensions', 'link',
                str(self.source_extensions_dir / 'sample_1')
            ]),
            unittest.mock.call([
                'gemini', 'extensions', 'disable', 'sample_1', '--scope=User'
            ]),
            unittest.mock.call([
                'gemini', 'extensions', 'enable', 'sample_1',
                '--scope=Workspace'
            ]),
        ]
        self.mock_run_command.assert_has_calls(calls)
        self.assertFalse(project_extensions_dir.exists())

    def test_fix_extensions_no_project_dir(self):
        """Tests fix command when no project-level directory exists."""
        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py', 'fix']):
                install.main()
            self.assertIn('No project-level extensions found to fix.',
                          mock_stdout.getvalue())

        self.mock_run_command.assert_not_called()

    def test_fix_extensions_no_extensions(self):
        """Tests fix command when no project-level extensions are found."""
        project_extensions_dir = self.project_root / '.gemini' / 'extensions'
        self.fs.create_dir(project_extensions_dir)

        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py', 'fix']):
                install.main()
            self.assertIn(
                'No valid project-level extensions found.',
                mock_stdout.getvalue(),
            )

        self.mock_run_command.assert_not_called()
        self.assertFalse(project_extensions_dir.exists())

    @unittest.mock.patch('pathlib.Path.home')
    def test_fix_skips_existing_user_extension(self, mock_home):
        """Tests that fix skips extensions that already exist for the user."""
        fake_home = Path(self.tmpdir) / 'home'
        mock_home.return_value = fake_home

        # Set up a user-level extension
        (install.get_global_extension_dir() / 'sample_1').mkdir(parents=True)

        # Create a project-level extension with the same name
        project_extensions_dir = self.project_root / '.gemini' / 'extensions'
        self.fs.create_dir(project_extensions_dir)
        self.fs.create_file(
            project_extensions_dir / 'sample_1' / 'gemini-extension.json',
            contents='{"name": "sample_1", "version": "1.0.0"}',
        )

        with unittest.mock.patch('sys.stderr',
                                 new_callable=io.StringIO) as mock_stderr:
            with unittest.mock.patch('sys.argv', ['install.py', 'fix']):
                install.main()
            self.assertIn(
                'Warning: User extension "sample_1" already exists.',
                mock_stderr.getvalue(),
            )

        self.mock_run_command.assert_not_called()
        self.assertFalse(project_extensions_dir.exists())

    def test_prompt_for_fix(self):
        """Tests that the user is prompted to run fix."""
        project_extensions_dir = self.project_root / '.gemini' / 'extensions'
        self.fs.create_dir(project_extensions_dir)
        with unittest.mock.patch('sys.stderr',
                                 new_callable=io.StringIO) as mock_stderr:
            with unittest.mock.patch('sys.argv', ['install.py', 'list']):
                install.main()
            self.assertIn('WARNING: Project-level extensions are deprecated.',
                          mock_stderr.getvalue())

    def test_get_project_root(self):
        """Tests the get_project_root function."""
        with unittest.mock.patch('install._PROJECT_ROOT', self.project_root):
            project_root = install.get_project_root()
            self.assertEqual(project_root, self.project_root)


if __name__ == '__main__':
    unittest.main()

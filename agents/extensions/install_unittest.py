#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for install.py."""

import io
import pathlib
import subprocess
import unittest
import unittest.mock

import install
from pyfakefs import fake_filesystem_unittest


class InstallTest(fake_filesystem_unittest.TestCase):
    """Tests for the extension installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.setUpPyfakefs(additional_skip_names=['subprocess'])
        self.tmpdir = '/tmp/test'
        self.project_root = pathlib.Path(self.tmpdir) / 'src'
        self.fs.create_dir(self.project_root)

        self.source_extensions_dir = self.project_root / 'agents' / 'extensions'
        self.fs.create_dir(self.source_extensions_dir)
        self.install_script_path = self.source_extensions_dir / 'install.py'
        self.fs.create_file(self.install_script_path)

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

        self.mock_run_command_patcher = unittest.mock.patch(
            'install._run_command')
        self.mock_run_command = self.mock_run_command_patcher.start()
        self.addCleanup(self.mock_run_command_patcher.stop)

        self.mock_check_version = unittest.mock.patch(
            'install.check_gemini_version')
        self.mock_check_version.start()
        self.addCleanup(self.mock_check_version.stop)

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

    def test_get_extensions_dirs(self):
        """Tests that get_extensions_dirs returns correct directories."""
        dirs = install.get_extensions_dirs(self.project_root)
        self.assertIn(self.source_extensions_dir, dirs)
        self.assertIn(self.internal_extensions_dir, dirs)

    def test_get_extensions_dirs_no_project_root(self):
        """Tests get_extensions_dirs() when no project root is found."""
        extensions_dirs = install.get_extensions_dirs(None)
        self.assertEqual(extensions_dirs, [])

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_copy(self, mock_find_dir, mock_get_project_root):
        """Tests add command with copy."""
        mock_get_project_root.return_value = self.project_root
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', '--copy', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with([
            'gemini', 'extensions', 'install', '--path',
            str(self.source_extensions_dir / 'sample_1')
        ])

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_link(self, mock_find_dir, mock_get_project_root):
        """Tests add command."""
        mock_get_project_root.return_value = self.project_root
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'add', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with([
            'gemini', 'extensions', 'link',
            str(self.source_extensions_dir / 'sample_1')
        ])

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_add_extension_skip_prompt(self, mock_find_dir,
                                       mock_get_project_root):
        """Tests that the skip_prompt flag is accepted."""
        mock_get_project_root.return_value = self.project_root
        mock_find_dir.return_value = self.source_extensions_dir
        with unittest.mock.patch(
                'sys.argv',
            ['install.py', 'add', '--skip-prompt', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with([
            'gemini', 'extensions', 'link',
            str(self.source_extensions_dir / 'sample_1')
        ])

    @unittest.mock.patch('install.get_project_root')
    def test_add_invalid_extension(self, mock_get_project_root):
        """Tests add command with an invalid extension."""
        mock_get_project_root.return_value = self.project_root
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

    @unittest.mock.patch('install.get_project_root')
    def test_update_extension(self, mock_get_project_root):
        """Tests update command."""
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'update', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'update', 'sample_1'])

    @unittest.mock.patch('install.get_project_root')
    def test_update_all_extensions(self, mock_get_project_root):
        """Tests update command with no extension specified."""
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.argv', ['install.py', 'update']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'update', '--all'])

    @unittest.mock.patch('install.get_project_root')
    def test_remove_extension(self, mock_get_project_root):
        """Tests remove command."""
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.argv',
                                 ['install.py', 'remove', 'sample_1']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'uninstall', 'sample_1'])

    @unittest.mock.patch('install.get_project_root')
    def test_list_extensions(self, mock_get_project_root):
        """Tests that list command calls gemini extensions list."""
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.argv', ['install.py', 'list']):
            install.main()
        self.mock_run_command.assert_called_once_with(
            ['gemini', 'extensions', 'list'])

    def test_find_extensions_dir_for_nonexistent_extension(self):
        """Tests finding a non-existent extension."""
        extensions_dirs = install.get_extensions_dirs(self.project_root)
        ext_dir = install.find_extensions_dir_for_extension(
            'nonexistent', extensions_dirs)
        self.assertIsNone(ext_dir)

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_fix_extensions(self, mock_find_dir, mock_get_project_root):
        """Tests fix command."""
        mock_get_project_root.return_value = self.project_root
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

    @unittest.mock.patch('install.get_project_root')
    def test_fix_extensions_no_project_dir(self, mock_get_project_root):
        """Tests fix command when no project-level directory exists."""
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py', 'fix']):
                install.main()
            self.assertIn('No project-level extensions found to fix.',
                          mock_stdout.getvalue())

        self.mock_run_command.assert_not_called()

    @unittest.mock.patch('install.get_project_root')
    def test_fix_extensions_no_extensions(self, mock_get_project_root):
        """Tests fix command when no project-level extensions are found."""
        mock_get_project_root.return_value = self.project_root
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
    @unittest.mock.patch('install.get_project_root')
    def test_fix_skips_existing_user_extension(self, mock_get_project_root,
                                               mock_home):
        """Tests that fix skips extensions that already exist for the user."""
        mock_get_project_root.return_value = self.project_root
        fake_home = pathlib.Path(self.tmpdir) / 'home'
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

    @unittest.mock.patch('install.get_project_root')
    def test_prompt_for_fix(self, mock_get_project_root):
        """Tests that the user is prompted to run fix."""
        mock_get_project_root.return_value = self.project_root
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
        with unittest.mock.patch('install.__file__', self.install_script_path):
            project_root = install.get_project_root()
            self.assertEqual(project_root, self.project_root)

    def test_get_project_root_error(self):
        """Tests the get_project_root function when an error occurs."""
        with unittest.mock.patch('install.__file__',
                                 pathlib.Path('invalid/path')):
            with unittest.mock.patch('sys.stderr',
                                     new_callable=io.StringIO) as mock_stderr:
                project_root = install.get_project_root()
                self.assertIsNone(project_root)
                self.assertIn('Could not determine project root',
                              mock_stderr.getvalue())

    @unittest.mock.patch('subprocess.run')
    def test_get_gemini_version_success(self, mock_run):
        """Test that we can successfully get the gemini version."""
        mock_run.return_value.stdout = '0.5.1'
        self.assertEqual(install.get_gemini_version(), '0.5.1')

    @unittest.mock.patch('subprocess.run', side_effect=FileNotFoundError)
    def test_get_gemini_version_file_not_found(self, _mock_run):
        """Test that we return none when gemini is not found."""
        self.assertIsNone(install.get_gemini_version())

    @unittest.mock.patch('subprocess.run',
                         side_effect=subprocess.CalledProcessError(1, 'cmd'))
    def test_get_gemini_version_called_process_error(self, _mock_run):
        """Test that we return none when there is a process error."""
        self.assertIsNone(install.get_gemini_version())


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for install.py."""

import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import install


class InstallTest(unittest.TestCase):
    """Tests for the extension installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.tmpdir = tempfile.mkdtemp()
        self.gclient_root = Path(self.tmpdir)
        self.project_root = self.gclient_root / 'src'
        self.project_root.mkdir()
        (self.project_root / '.gn').touch()
        (self.project_root / 'DEPS').touch()

        self.source_extensions_dir = self.project_root / 'agents' / 'extensions'
        self.install_script_path = self.source_extensions_dir / 'install.py'
        self.install_script_path.parent.mkdir(parents=True, exist_ok=True)
        self.install_script_path.touch()

        self.target_extensions_dir = (self.project_root / '.gemini' /
                                        'extensions')
        self.target_extensions_dir.mkdir(parents=True)
        self.global_extension_dir = Path(
            self.tmpdir) / 'home' / '.gemini' / 'extensions'
        self.global_extension_dir.mkdir(parents=True)

        # Create sample extensions
        self.extension1_dir = self.source_extensions_dir / 'sample_1'
        self.extension1_dir.mkdir()
        with open(self.extension1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_1", "version": "1.0.0"}')
        with open(self.extension1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("hello")')

        self.extension2_dir = self.source_extensions_dir / 'sample_2'
        self.extension2_dir.mkdir()
        with open(self.extension2_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_2", "version": "2.0.0"}')

        self.internal_extensions_dir = (self.project_root / 'internal' /
                                        'agents' / 'extensions')
        self.internal_extensions_dir.mkdir(parents=True)
        self.extension3_dir = self.internal_extensions_dir / 'sample_3'
        self.extension3_dir.mkdir()
        with open(self.extension3_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_3", "version": "3.0.0"}')

    def tearDown(self):
        """Tears down the test environment."""
        shutil.rmtree(self.tmpdir)

    def test_get_dir_hash(self):
        """Tests the get_dir_hash function."""
        hash1 = install.get_dir_hash(self.extension1_dir)
        hash2 = install.get_dir_hash(self.extension1_dir)
        self.assertEqual(hash1, hash2)

        # Test that a change in content changes the hash
        with open(self.extension1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("world")')
        hash3 = install.get_dir_hash(self.extension1_dir)
        self.assertNotEqual(hash1, hash3)

    def test_find_extensions_dir_for_extension(self):
        """Tests the find_extensions_dir_for_extension function."""
        extensions_dirs = [
            self.source_extensions_dir, self.internal_extensions_dir
        ]
        self.assertEqual(
            install.find_extensions_dir_for_extension('sample_1',
                                                      extensions_dirs),
            self.source_extensions_dir)
        self.assertEqual(
            install.find_extensions_dir_for_extension('sample_3',
                                                      extensions_dirs),
            self.internal_extensions_dir)
        self.assertIsNone(
            install.find_extensions_dir_for_extension('non_existent_extension',
                                                      extensions_dirs))

    def test_get_extensions_dirs(self):
        """Tests the get_extensions_dirs function."""
        extensions_dirs = install.get_extensions_dirs(self.project_root)
        self.assertIn(self.source_extensions_dir, extensions_dirs)
        self.assertIn(self.internal_extensions_dir, extensions_dirs)

    def test_get_extensions_dirs_no_project_root(self):
        """Tests get_extensions_dirs() when no project root is found."""
        extensions_dirs = install.get_extensions_dirs(None)
        self.assertEqual(extensions_dirs, [])

    def test_get_dir_hash_ignores_tests(self):
        """Tests that get_dir_hash ignores the 'tests' directory."""
        # Create a tests directory and get the hash
        (self.extension1_dir / 'tests').mkdir()
        with open(self.extension1_dir / 'tests' / 'test.py',
                  'w',
                  encoding='utf-8') as f:
            f.write('print("test")')
        hash1 = install.get_dir_hash(self.extension1_dir)

        # Modify a file in the tests directory and check the hash is the same
        with open(self.extension1_dir / 'tests' / 'test.py',
                  'w',
                  encoding='utf-8') as f:
            f.write('print("test2")')
        hash2 = install.get_dir_hash(self.extension1_dir)
        self.assertEqual(hash1, hash2)

        # Modify a file outside the tests directory and check the hash changes
        with open(self.extension1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("new world")')
        hash3 = install.get_dir_hash(self.extension1_dir)
        self.assertNotEqual(hash1, hash3)

    @patch('subprocess.check_output', side_effect=FileNotFoundError)
    def test_get_dir_hash_fallback(self, mock_check_output):
        """Tests the get_dir_hash function's fallback mechanism."""
        hash1 = install.get_dir_hash(self.extension1_dir)
        hash2 = install.get_dir_hash(self.extension1_dir)
        self.assertEqual(hash1, hash2)
        self.assertIsNotNone(hash1)

    @patch('install.get_dir_hash')
    def test_is_up_to_date(self, mock_get_dir_hash):
        """Tests the is_up_to_date() function."""
        mock_get_dir_hash.return_value = b'some_hash'
        self.assertFalse(
            install.is_up_to_date('sample_1', self.source_extensions_dir,
                                  self.target_extensions_dir))

        install.add_extension('sample_1', self.source_extensions_dir,
                              self.target_extensions_dir, False)
        mock_get_dir_hash.side_effect = [b'some_hash', b'some_hash']
        self.assertTrue(
            install.is_up_to_date('sample_1', self.source_extensions_dir,
                                  self.target_extensions_dir))

        mock_get_dir_hash.side_effect = [b'new_hash', b'old_hash']
        self.assertFalse(
            install.is_up_to_date('sample_1', self.source_extensions_dir,
                                  self.target_extensions_dir))

    @patch('builtins.input', return_value='y')
    def test_add_extension(self, mock_input):
        """Tests the add_extension function."""
        install.add_extension('sample_1', self.source_extensions_dir,
                              self.target_extensions_dir, False)
        self.assertTrue((self.target_extensions_dir / 'sample_1').exists())

    @patch('builtins.input', return_value='y')
    def test_add_extension_symlink(self, mock_input):
        """Tests the add_extension function with symlinking."""
        install.add_extension('sample_1', self.source_extensions_dir,
                              self.target_extensions_dir, True)
        self.assertTrue((self.target_extensions_dir / 'sample_1').is_symlink())

    @patch('builtins.input', return_value='n')
    @patch('install.is_up_to_date', return_value=False)
    def test_add_extension_decline_update(self, mock_is_up_to_date,
                                          mock_input):
        """Adding an existing extension is skipped if the user declines."""
        install.add_extension('sample_1', self.source_extensions_dir,
                              self.target_extensions_dir, False)
        with patch('shutil.copytree') as mock_copy:
            install.add_extension('sample_1', self.source_extensions_dir,
                                  self.target_extensions_dir, False)
            mock_copy.assert_not_called()

    def test_update_extension(self):
        """Tests the update_extension function."""
        # Test updating a non-existent extension
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            install.update_extension('sample_1', self.source_extensions_dir,
                                     self.target_extensions_dir)
            self.assertIn('not installed', mock_stderr.getvalue())

        # Test updating an up-to-date extension
        install.add_extension('sample_1', self.source_extensions_dir,
                              self.target_extensions_dir, False)
        with patch('install.is_up_to_date', return_value=True):
            with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
                install.update_extension('sample_1',
                                         self.source_extensions_dir,
                                         self.target_extensions_dir)
                self.assertIn('already up to date', mock_stdout.getvalue())

    def test_remove_extension_not_installed(self):
        """Tests removing a extension that is not installed."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            install.remove_extension('sample_1', self.target_extensions_dir)
            self.assertIn('not found', mock_stderr.getvalue())

    @patch('install.get_project_root')
    @patch('install.get_extension_dir')
    @patch('install.add_extension')
    @patch('install.find_extensions_dir_for_extension')
    def test_main_add_global(self, mock_find_extensions, mock_add_extension,
                             mock_get_extension_dir, mock_get_project_root):
        """Tests the main function with the add command and --global flag."""
        mock_find_extensions.return_value = self.source_extensions_dir
        mock_get_extension_dir.return_value = self.global_extension_dir
        mock_get_project_root.return_value = self.project_root
        with patch('sys.argv', ['install.py', 'add', '-g', 'sample_1']):
            install.main()
        mock_add_extension.assert_called_once_with('sample_1',
                                                   self.source_extensions_dir,
                                                   self.global_extension_dir,
                                                   False)

    @patch('install.get_project_root')
    @patch('install.update_extension')
    @patch('install.get_installed_extensions', return_value=['sample_1'])
    @patch('install.find_extensions_dir_for_extension')
    def test_main_update_all(self, mock_find_extensions, mock_get_installed,
                             mock_update_extension, mock_get_project_root):
        """Tests the main function with the update command and no extensions."""
        mock_find_extensions.return_value = self.source_extensions_dir
        mock_get_project_root.return_value = self.project_root
        with patch('sys.argv', ['install.py', 'update']):
            install.main()
        mock_update_extension.assert_called_once()

    @patch('sys.stderr', new_callable=io.StringIO)
    @patch('install.get_project_root')
    @patch('install.find_extensions_dir_for_extension')
    def test_main_invalid_extension(self, mock_find_extensions,
                                    mock_get_project_root, mock_stderr):
        """Tests that main handles invalid extension names gracefully."""
        mock_get_project_root.return_value = self.project_root
        mock_find_extensions.return_value = None
        with patch('sys.argv', ['install.py', 'add', 'invalid_extension']):
            with patch('install.get_extensions_dirs',
                       return_value=[self.source_extensions_dir]):
                install.main()
        self.assertIn("Error: Extension 'invalid_extension' not found",
                      mock_stderr.getvalue())

    @patch('install.get_project_root', return_value=None)
    def test_main_no_project_root(self, mock_get_project_root):
        """Tests that main exits if no project root is found for a local op."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv', ['install.py', 'add', 'sample_1']):
                with self.assertRaises(SystemExit):
                    install.main()
            self.assertIn('Could not determine target directory',
                          mock_stderr.getvalue())

    def test_list_extensions_excludes_example_server(self):
        """Tests that the list_extensions function excludes 'example_server'."""
        # Create an example_server extension
        example_server_dir = self.source_extensions_dir / 'example_server'
        example_server_dir.mkdir()
        with open(example_server_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "example_server", "version": "1.0.0"}')

        extensions_dirs = [
            self.source_extensions_dir, self.internal_extensions_dir
        ]

        with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
            with patch('install.get_extension_dir',
                       return_value=self.target_extensions_dir):
                install.list_extensions(self.project_root, extensions_dirs)
                output = mock_stdout.getvalue()
                self.assertNotIn('example_server', output)
                self.assertIn('sample_1', output)
                self.assertIn('sample_2', output)
                self.assertIn('sample_3', output)

    def test_get_project_root(self):
        """Tests the get_project_root function."""
        with patch('install.__file__', self.install_script_path):
            project_root = install.get_project_root()
            self.assertEqual(project_root, self.project_root)

    def test_get_project_root_error(self):
        """Tests the get_project_root function when an error occurs."""
        with patch('install.__file__', Path('invalid/path')):
            with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
                project_root = install.get_project_root()
                self.assertIsNone(project_root)
                self.assertIn('Could not determine project root',
                              mock_stderr.getvalue())


if __name__ == '__main__':
    unittest.main()

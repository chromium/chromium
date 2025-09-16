#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for install.py."""

import io
import pathlib
import unittest
import unittest.mock

import install
from pyfakefs import fake_filesystem_unittest


class InstallTest(fake_filesystem_unittest.TestCase):
    """Tests for the extension installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.setUpPyfakefs(additional_skip_names=["subprocess"])
        self.tmpdir = '/tmp/test'
        self.project_root = pathlib.Path(self.tmpdir) / 'src'
        self.fs.create_dir(self.project_root)

        self.source_extensions_dir = self.project_root / 'agents' / 'extensions'
        self.fs.create_dir(self.source_extensions_dir)
        self.install_script_path = self.source_extensions_dir / 'install.py'
        self.fs.create_file(self.install_script_path)

        self.target_extensions_dir = (
            self.project_root / '.gemini' / 'extensions'
        )
        self.fs.create_dir(self.target_extensions_dir)
        self.global_extension_dir = (
            pathlib.Path(self.tmpdir) / 'home' / '.gemini' / 'extensions'
        )
        self.fs.create_dir(self.global_extension_dir)

        # Create sample extensions
        self.extension1_dir = self.source_extensions_dir / 'sample_1'
        self.fs.create_dir(self.extension1_dir)
        self.fs.create_file(
            self.extension1_dir / 'gemini-extension.json',
            contents='{"name": "sample_1", "version": "1.0.0"}',
        )
        self.fs.create_file(
            self.extension1_dir / 'main.py', contents='print("hello")'
        )

        self.extension2_dir = self.source_extensions_dir / 'sample_2'
        self.fs.create_dir(self.extension2_dir)
        self.fs.create_file(
            self.extension2_dir / 'gemini-extension.json',
            contents='{"name": "sample_2", "version": "2.0.0"}',
        )

        self.internal_extensions_dir = (
            self.project_root / 'internal' / 'agents' / 'extensions'
        )
        self.fs.create_dir(self.internal_extensions_dir)
        self.extension3_dir = self.internal_extensions_dir / 'sample_3'
        self.fs.create_dir(self.extension3_dir)
        self.fs.create_file(
            self.extension3_dir / 'gemini-extension.json',
            contents='{"name": "sample_3", "version": "3.0.0"}',
        )

    @unittest.mock.patch(
        'subprocess.check_output', side_effect=FileNotFoundError
    )
    def test_get_dir_hash(self, _):
        """Tests the get_dir_hash function's fallback mechanism."""
        hash1 = install.get_dir_hash(self.extension1_dir)
        hash2 = install.get_dir_hash(self.extension1_dir)
        self.assertEqual(hash1, hash2)
        self.assertIsNotNone(hash1)

        # Test that a change in content changes the hash
        with open(self.extension1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("world")')
        hash3 = install.get_dir_hash(self.extension1_dir)
        self.assertNotEqual(hash1, hash3)

    def test_find_extensions_dir_for_extension(self):
        """Tests the find_extensions_dir_for_extension function."""
        extensions_dirs = [
            self.source_extensions_dir,
            self.internal_extensions_dir,
        ]
        self.assertEqual(
            install.find_extensions_dir_for_extension(
                'sample_1', extensions_dirs
            ),
            self.source_extensions_dir,
        )
        self.assertEqual(
            install.find_extensions_dir_for_extension(
                'sample_3', extensions_dirs
            ),
            self.internal_extensions_dir,
        )
        self.assertIsNone(
            install.find_extensions_dir_for_extension(
                'non_existent_extension', extensions_dirs
            )
        )

    def test_get_extensions_dirs(self):
        """Tests the get_extensions_dirs function."""
        with unittest.mock.patch('install.__file__', self.install_script_path):
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
        self.fs.create_dir(self.extension1_dir / 'tests')
        self.fs.create_file(
            self.extension1_dir / 'tests' / 'test.py', contents='print("test")'
        )
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

    @unittest.mock.patch(
        'subprocess.check_output', side_effect=FileNotFoundError
    )
    def test_get_dir_hash_fallback(self, _):
        """Tests the get_dir_hash function's fallback mechanism."""
        hash1 = install.get_dir_hash(self.extension1_dir)
        hash2 = install.get_dir_hash(self.extension1_dir)
        self.assertEqual(hash1, hash2)
        self.assertIsNotNone(hash1)

    @unittest.mock.patch('install.get_dir_hash')
    def test_is_up_to_date(self, mock_get_dir_hash):
        """Tests the is_up_to_date() function."""
        mock_get_dir_hash.return_value = b'some_hash'
        self.assertFalse(
            install.is_up_to_date(
                'sample_1',
                self.source_extensions_dir,
                self.target_extensions_dir,
            )
        )

        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            False,
        )
        mock_get_dir_hash.side_effect = [b'some_hash', b'some_hash']
        self.assertTrue(
            install.is_up_to_date(
                'sample_1',
                self.source_extensions_dir,
                self.target_extensions_dir,
            )
        )

        mock_get_dir_hash.side_effect = [b'new_hash', b'old_hash']
        self.assertFalse(
            install.is_up_to_date(
                'sample_1',
                self.source_extensions_dir,
                self.target_extensions_dir,
            )
        )

    @unittest.mock.patch('builtins.input', return_value='y')
    def test_add_extension_copy(self, _):
        """Tests the add_extension function with copying."""
        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            False,
        )
        dest_path = self.target_extensions_dir / 'sample_1'
        self.assertTrue(dest_path.exists())
        self.assertFalse(dest_path.is_symlink())

    @unittest.mock.patch('builtins.input', return_value='y')
    def test_add_extension_symlink(self, _):
        """Tests the add_extension function with symlinking."""
        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            True,
        )
        self.assertTrue(
            (self.target_extensions_dir / 'sample_1').is_symlink()
        )

    @unittest.mock.patch('builtins.input', return_value='n')
    @unittest.mock.patch('install.is_up_to_date', return_value=False)
    def test_add_extension_decline_update(self, _, __):
        """Adding an existing extension is skipped if the user declines."""
        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            False,
        )
        with unittest.mock.patch('shutil.copytree') as mock_copy:
            install.add_extension(
                'sample_1',
                self.source_extensions_dir,
                self.target_extensions_dir,
                False,
            )
            mock_copy.assert_not_called()

    @unittest.mock.patch('builtins.input')
    @unittest.mock.patch('install.is_up_to_date', return_value=False)
    def test_add_extension_skip_prompt(self, _, mock_input):
        """Tests that the skip_prompt flag works correctly."""
        dest_path = self.target_extensions_dir / 'sample_1'
        dest_path.mkdir()
        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            symlink=False,
            skip_prompt=True,
        )
        mock_input.assert_not_called()
        self.assertFalse(dest_path.is_symlink())

    def test_update_extension(self):
        """Tests the update_extension function."""
        # Test updating a non-existent extension
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            install.update_extension(
                'sample_1',
                self.source_extensions_dir,
                self.target_extensions_dir,
            )
            self.assertIn('not installed', mock_stderr.getvalue())

        # Test updating an up-to-date extension
        install.add_extension(
            'sample_1',
            self.source_extensions_dir,
            self.target_extensions_dir,
            False,
        )
        with unittest.mock.patch('install.is_up_to_date', return_value=True):
            with unittest.mock.patch(
                'sys.stdout', new_callable=io.StringIO
            ) as mock_stdout:
                install.update_extension(
                    'sample_1',
                    self.source_extensions_dir,
                    self.target_extensions_dir,
                )
                self.assertIn('already up to date', mock_stdout.getvalue())

    def test_remove_extension_not_installed(self):
        """Tests removing a extension that is not installed."""
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            install.remove_extension('sample_1', self.target_extensions_dir)
            self.assertIn('not found', mock_stderr.getvalue())

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.get_extension_dir')
    @unittest.mock.patch('install.add_extension')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_main_add_global(
        self,
        mock_find_extensions,
        mock_add_extension,
        mock_get_extension_dir,
        mock_get_project_root,
    ):
        """Tests the main function with the add command and --global flag."""
        mock_find_extensions.return_value = self.source_extensions_dir
        mock_get_extension_dir.return_value = self.global_extension_dir
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', '-g', 'sample_1']
        ):
            install.main()
        mock_add_extension.assert_called_once_with(
            'sample_1',
            self.source_extensions_dir,
            self.global_extension_dir,
            symlink=True,
            skip_prompt=False,
        )

    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.update_extension')
    @unittest.mock.patch(
        'install.get_installed_extensions', return_value=['sample_1']
    )
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_main_update_all(
        self,
        mock_find_extensions,
        _,
        mock_update_extension,
        mock_get_project_root,
    ):
        """Tests the main function with the update command and no extensions."""
        mock_find_extensions.return_value = self.source_extensions_dir
        mock_get_project_root.return_value = self.project_root
        with unittest.mock.patch('sys.argv', ['install.py', 'update']):
            install.main()
        mock_update_extension.assert_called_once()

    @unittest.mock.patch('sys.stderr', new_callable=io.StringIO)
    @unittest.mock.patch('install.get_project_root')
    @unittest.mock.patch('install.find_extensions_dir_for_extension')
    def test_main_invalid_extension(
        self,
        mock_find_extensions,
        mock_get_project_root,
        mock_stderr,
    ):
        """Tests that main handles invalid extension names gracefully."""
        mock_get_project_root.return_value = self.project_root
        mock_find_extensions.return_value = None
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'invalid_extension']
        ):
            with unittest.mock.patch(
                'install.get_extensions_dirs',
                return_value=[self.source_extensions_dir],
            ):
                install.main()
        self.assertIn(
            "Error: Extension 'invalid_extension' not found",
            mock_stderr.getvalue(),
        )

    @unittest.mock.patch('install.get_project_root', return_value=None)
    def test_main_no_project_root(self, _):
        """Tests that main exits if no project root is found for a local op."""
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'add', 'sample_1']
            ):
                with self.assertRaises(SystemExit):
                    install.main()
            self.assertIn(
                'Could not determine target directory', mock_stderr.getvalue()
            )

    def test_list_extensions_excludes_example_server(self):
        """Tests that the list_extensions function excludes 'example_server'."""
        # Create an example_server extension
        example_server_dir = self.source_extensions_dir / 'example_server'
        self.fs.create_dir(example_server_dir)
        self.fs.create_file(
            example_server_dir / 'gemini-extension.json',
            contents='{"name": "example_server", "version": "1.0.0"}',
        )

        extensions_dirs = [
            self.source_extensions_dir,
            self.internal_extensions_dir,
        ]

        with unittest.mock.patch(
            'sys.stdout', new_callable=io.StringIO
        ) as mock_stdout:
            with unittest.mock.patch(
                'install.get_extension_dir',
                return_value=self.target_extensions_dir,
            ):
                install.list_extensions(self.project_root, extensions_dirs)
                output = mock_stdout.getvalue()
                self.assertNotIn('example_server', output)
                self.assertIn('sample_1', output)
                self.assertIn('sample_2', output)
                self.assertIn('sample_3', output)

    def test_get_project_root(self):
        """Tests the get_project_root function."""
        with unittest.mock.patch('install.__file__', self.install_script_path):
            project_root = install.get_project_root()
            self.assertEqual(project_root, self.project_root)

    def test_get_project_root_error(self):
        """Tests the get_project_root function when an error occurs."""
        with unittest.mock.patch(
            'install.__file__', pathlib.Path('invalid/path')
        ):
            with unittest.mock.patch(
                'sys.stderr', new_callable=io.StringIO
            ) as mock_stderr:
                project_root = install.get_project_root()
                self.assertIsNone(project_root)
                self.assertIn(
                    'Could not determine project root', mock_stderr.getvalue()
                )


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for install.py."""

import io
import os
import pathlib
import unittest
import unittest.mock

import install
from pyfakefs import fake_filesystem_unittest


class InstallIntegrationTest(fake_filesystem_unittest.TestCase):
    """Integration tests for the install.py."""

    def setUp(self):
        """Sets up the test environment."""
        self.setUpPyfakefs(additional_skip_names=["subprocess"])
        self.tmpdir = '/tmp/test'
        self.project_root = pathlib.Path(self.tmpdir) / 'src'
        self.source_extensions_dir = (
            self.project_root / 'agents' / 'extensions'
        )
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
        self.fs.create_dir(self.extension1_dir / 'tests')
        self.fs.create_file(
            self.extension1_dir / 'tests' / 'test.py',
            contents='print("hello")',
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

        # Patch the script's dependencies
        self.mock_extensions_dirs = unittest.mock.patch(
            'install.get_extensions_dirs',
            return_value=[
                self.source_extensions_dir,
                self.internal_extensions_dir,
            ],
        )
        self.mock_install_file = unittest.mock.patch(
            'install.__file__', self.install_script_path
        )
        self.mock_home = unittest.mock.patch(
            'pathlib.Path.home',
            return_value=pathlib.Path(self.tmpdir) / 'home',
        )

        self.mock_extensions_dirs.start()
        self.mock_install_file.start()
        self.mock_home.start()
        self.addCleanup(self.mock_extensions_dirs.stop)
        self.addCleanup(self.mock_install_file.stop)
        self.addCleanup(self.mock_home.stop)

    def test_list_from_multiple_sources(self):
        """Tests listing extensions from multiple source directories."""
        with unittest.mock.patch(
            'sys.stdout', new_callable=io.StringIO
        ) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py', 'list']):
                install.main()
            output = mock_stdout.getvalue()
            self.assertIn('sample_1', output)
            self.assertIn('sample_2', output)
            self.assertIn('sample_3', output)

    def test_add_from_multiple_sources(self):
        """Tests adding extensions from different source directories."""
        # Add extension from the primary source_extensions_dir
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_1']
        ):
            install.main()
        self.assertTrue(
            (self.target_extensions_dir / 'sample_1').exists()
        )

        # Add extension from the internal_extensions_dir
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_3']
        ):
            install.main()
        self.assertTrue(
            (self.target_extensions_dir / 'sample_3').exists()
        )

    def test_add_remove_sequence(self):
        """Tests adding and then removing a extension."""
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_1']
        ):
            install.main()
        self.assertTrue(
            (self.target_extensions_dir / 'sample_1').exists()
        )
        self.assertFalse(
            (self.target_extensions_dir / 'sample_1' / 'tests').exists()
        )

        with unittest.mock.patch(
            'sys.argv', ['install.py', 'remove', 'sample_1']
        ):
            install.main()
        self.assertFalse(
            (self.target_extensions_dir / 'sample_1').exists()
        )

    def test_symlink_add_remove(self):
        """Tests adding and removing a symlinked extension."""
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', '--symlink', 'sample_1']
        ):
            install.main()
        symlink_path = self.target_extensions_dir / 'sample_1'
        self.assertTrue(symlink_path.is_symlink())
        # Windows adds \\?\ as a prefix for readlink().
        self.assertEqual(
            os.readlink(symlink_path).removeprefix('\\?\\'),
            str(self.extension1_dir),
        )

        with unittest.mock.patch(
            'sys.argv', ['install.py', 'remove', 'sample_1']
        ):
            install.main()
        self.assertFalse(symlink_path.exists())

    def test_update_sequence(self):
        """Tests adding, updating, and then checking the version."""
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_1']
        ):
            install.main()

        # Check that changes in the tests directory don't trigger an update
        with open(
            self.extension1_dir / 'tests' / 'test.py',
            'w',
            encoding='utf-8',
        ) as f:
            f.write('print("goodbye")')
        with unittest.mock.patch(
            'sys.stdout', new_callable=io.StringIO
        ) as mock_stdout:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'update', 'sample_1']
            ):
                install.main()
            self.assertIn('already up to date', mock_stdout.getvalue())

        # Check that a legitimate change does trigger an update
        with open(
            self.extension1_dir / 'gemini-extension.json',
            'w',
            encoding='utf-8',
        ) as f:
            f.write('{"name": "sample_1", "version": "1.0.1"}')

        with unittest.mock.patch(
            'sys.argv', ['install.py', 'update', 'sample_1']
        ):
            install.main()

        version = install.get_extension_version(
            self.target_extensions_dir / 'sample_1'
        )
        self.assertEqual(version, '1.0.1')

    def test_update_all_extensions(self):
        """Tests updating all installed extensions at once."""
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_1']
        ):
            install.main()
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', 'sample_2']
        ):
            install.main()

        with open(
            self.extension1_dir / 'gemini-extension.json',
            'w',
            encoding='utf-8',
        ) as f:
            f.write('{"name": "sample_1", "version": "1.0.1"}')
        with open(
            self.extension2_dir / 'gemini-extension.json',
            'w',
            encoding='utf-8',
        ) as f:
            f.write('{"name": "sample_2", "version": "2.0.1"}')

        with unittest.mock.patch('sys.argv', ['install.py', 'update']):
            install.main()

        v1 = install.get_extension_version(
            self.target_extensions_dir / 'sample_1'
        )
        v2 = install.get_extension_version(
            self.target_extensions_dir / 'sample_2'
        )
        self.assertEqual(v1, '1.0.1')
        self.assertEqual(v2, '2.0.1')

    def test_global_commands(self):
        """Tests add, update, and remove with the --global flag."""
        # Add
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'add', '--global', 'sample_1']
        ):
            install.main()
        self.assertTrue(
            (self.global_extension_dir / 'sample_1').exists()
        )

        # Update
        with open(
            self.extension1_dir / 'gemini-extension.json',
            'w',
            encoding='utf-8',
        ) as f:
            f.write('{"name": "sample_1", "version": "1.0.1"}')
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'update', '--global', 'sample_1']
        ):
            install.main()
        v = install.get_extension_version(
            self.global_extension_dir / 'sample_1'
        )
        self.assertEqual(v, '1.0.1')

        # Remove
        with unittest.mock.patch(
            'sys.argv', ['install.py', 'remove', '--global', 'sample_1']
        ):
            install.main()
        self.assertFalse(
            (self.global_extension_dir / 'sample_1').exists()
        )

    def test_invalid_extension_names(self):
        """Tests that invalid extension names are handled gracefully."""
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'add', 'invalid_extension']
            ):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'update', 'invalid_extension']
            ):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'remove', 'invalid_extension']
            ):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

    def test_stateful_errors(self):
        """Tests errors that depend on the current installation state."""
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'update', 'sample_1']
            ):
                install.main()
            self.assertIn('not installed', mock_stderr.getvalue())

        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with unittest.mock.patch(
                'sys.argv', ['install.py', 'remove', 'sample_1']
            ):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

    def test_default_command(self):
        """Tests that the script defaults to --help."""
        with unittest.mock.patch(
            'sys.stdout', new_callable=io.StringIO
        ) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py']):
                with self.assertRaises(SystemExit):
                    install.main()
            self.assertIn('usage:', mock_stdout.getvalue())

    def test_missing_extension_arguments(self):
        """Tests that missing extension arguments are handled."""
        with unittest.mock.patch(
            'sys.stderr', new_callable=io.StringIO
        ) as mock_stderr:
            with self.assertRaises(SystemExit):
                with unittest.mock.patch(
                    'sys.argv', ['install.py', 'add']
                ):
                    install.main()
            self.assertIn(
                'the following arguments are required',
                mock_stderr.getvalue(),
            )

    def test_corrupted_extension_dir(self):
        """Tests that a corrupted extension directory is handled."""
        self.fs.create_dir(self.target_extensions_dir / 'corrupted_extension')
        with unittest.mock.patch(
            'sys.stdout', new_callable=io.StringIO
        ) as mock_stdout:
            with unittest.mock.patch('sys.argv', ['install.py', 'list']):
                install.main()
            self.assertNotIn('corrupted_extension', mock_stdout.getvalue())

    def test_empty_source_directory(self):
        """Tests behavior with an empty source directory."""
        empty_extensions_dir = pathlib.Path(self.tmpdir) / 'empty_extensions'
        self.fs.create_dir(empty_extensions_dir)
        with unittest.mock.patch(
            'install.get_extensions_dirs', return_value=[empty_extensions_dir]
        ):
            # Test list command
            with unittest.mock.patch(
                'sys.stdout', new_callable=io.StringIO
            ) as mock_stdout:
                with unittest.mock.patch(
                    'sys.argv', ['install.py', 'list']
                ):
                    install.main()
                self.assertIn('Extension', mock_stdout.getvalue())
                self.assertNotIn('sample_1', mock_stdout.getvalue())

            # Test add command
            with unittest.mock.patch(
                'sys.stderr', new_callable=io.StringIO
            ) as mock_stderr:
                with unittest.mock.patch(
                    'sys.argv', ['install.py', 'add', 'any_extension']
                ):
                    install.main()
                self.assertIn('not found', mock_stderr.getvalue())


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for install.py.

These tests verify the end-to-end functionality of the MCP server
configuration management script.
"""

import unittest
import os
import shutil
import tempfile
import io
from pathlib import Path
from unittest.mock import patch

import install


class InstallIntegrationTest(unittest.TestCase):
    """Integration tests for the MCP server installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.tmpdir = tempfile.mkdtemp()
        self.mcp_dir = Path(self.tmpdir) / 'agents' / 'mcp'
        self.mcp_dir.mkdir(parents=True)
        self.extension_dir = Path(self.tmpdir) / '.gemini' / 'extensions'
        self.extension_dir.mkdir(parents=True)
        self.global_extension_dir = Path(
            self.tmpdir) / 'home' / '.gemini' / 'extensions'
        self.global_extension_dir.mkdir(parents=True, exist_ok=True)

        # Create sample servers
        self.server1_dir = self.mcp_dir / 'sample_server_1'
        self.server1_dir.mkdir()
        with open(self.server1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_1", "version": "1.0.0"}')

        self.server2_dir = self.mcp_dir / 'sample_server_2'
        self.server2_dir.mkdir()
        with open(self.server2_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_2", "version": "2.0.0"}')

        # Patch the script's dependencies
        self.mock_mcp_dir = patch('install.Path.resolve',
                                  return_value=self.mcp_dir)
        self.mock_git_root = patch('install.get_git_repo_root',
                                   return_value=Path(self.tmpdir))
        self.mock_home = patch('pathlib.Path.home',
                               return_value=Path(self.tmpdir) / 'home')

        self.mock_mcp_dir.start()
        self.mock_git_root.start()
        self.mock_home.start()

    def tearDown(self):
        """Tears down the test environment."""
        shutil.rmtree(self.tmpdir)
        self.mock_mcp_dir.stop()
        self.mock_git_root.stop()
        self.mock_home.stop()

    def test_add_remove_sequence(self):
        """Tests adding and then removing a server."""
        with patch('sys.argv', ['install.py', 'add', 'sample_server_1']):
            install.main()
        self.assertTrue((self.extension_dir / 'sample_server_1').exists())

        with patch('sys.argv', ['install.py', 'remove', 'sample_server_1']):
            install.main()
        self.assertFalse((self.extension_dir / 'sample_server_1').exists())

    def test_update_sequence(self):
        """Tests adding, updating, and then checking the version."""
        with patch('sys.argv', ['install.py', 'add', 'sample_server_1']):
            install.main()

        with open(self.server1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_1", "version": "1.0.1"}')

        with patch('sys.argv', ['install.py', 'update', 'sample_server_1']):
            install.main()

        version = install.get_server_version(self.extension_dir /
                                             'sample_server_1')
        self.assertEqual(version, '1.0.1')

    def test_update_all_servers(self):
        """Tests updating all installed servers at once."""
        with patch('sys.argv', ['install.py', 'add', 'sample_server_1']):
            install.main()
        with patch('sys.argv', ['install.py', 'add', 'sample_server_2']):
            install.main()

        with open(self.server1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_1", "version": "1.0.1"}')
        with open(self.server2_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_2", "version": "2.0.1"}')

        with patch('sys.argv', ['install.py', 'update']):
            install.main()

        v1 = install.get_server_version(self.extension_dir / 'sample_server_1')
        v2 = install.get_server_version(self.extension_dir / 'sample_server_2')
        self.assertEqual(v1, '1.0.1')
        self.assertEqual(v2, '2.0.1')

    def test_global_commands(self):
        """Tests add, update, and remove with the --global flag."""
        # Add
        with patch('sys.argv',
                   ['install.py', 'add', '--global', 'sample_server_1']):
            install.main()
        self.assertTrue(
            (self.global_extension_dir / 'sample_server_1').exists())

        # Update
        with open(self.server1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_1", "version": "1.0.1"}')
        with patch('sys.argv',
                   ['install.py', 'update', '--global', 'sample_server_1']):
            install.main()
        v = install.get_server_version(self.global_extension_dir /
                                       'sample_server_1')
        self.assertEqual(v, '1.0.1')

        # Remove
        with patch('sys.argv',
                   ['install.py', 'remove', '--global', 'sample_server_1']):
            install.main()
        self.assertFalse(
            (self.global_extension_dir / 'sample_server_1').exists())

    def test_invalid_server_names(self):
        """Tests that invalid server names are handled gracefully."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv', ['install.py', 'add', 'invalid_server']):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv', ['install.py', 'update', 'invalid_server']):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv', ['install.py', 'remove', 'invalid_server']):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

    def test_stateful_errors(self):
        """Tests errors that depend on the current installation state."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv',
                       ['install.py', 'update', 'sample_server_1']):
                install.main()
            self.assertIn('not installed', mock_stderr.getvalue())

        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with patch('sys.argv',
                       ['install.py', 'remove', 'sample_server_1']):
                install.main()
            self.assertIn('not found', mock_stderr.getvalue())

    def test_default_command(self):
        """Tests that the script defaults to the list command."""
        with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
            with patch('sys.argv', ['install.py']):
                install.main()
            self.assertIn('MCP Server', mock_stdout.getvalue())

    def test_missing_server_arguments(self):
        """Tests that missing server arguments are handled."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            with self.assertRaises(SystemExit):
                with patch('sys.argv', ['install.py', 'add']):
                    install.main()
            self.assertIn('the following arguments are required',
                          mock_stderr.getvalue())

    def test_corrupted_extension_dir(self):
        """Tests that a corrupted extension directory is handled."""
        (self.extension_dir / 'corrupted_server').mkdir()
        with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
            with patch('sys.argv', ['install.py', 'list']):
                install.main()
            self.assertNotIn('corrupted_server', mock_stdout.getvalue())

    def test_empty_source_directory(self):
        """Tests behavior with an empty source directory."""
        empty_mcp_dir = Path(self.tmpdir) / 'empty_mcp'
        empty_mcp_dir.mkdir()
        with patch('install.Path.resolve', return_value=empty_mcp_dir):
            # Test list command
            with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
                with patch('sys.argv', ['install.py', 'list']):
                    install.main()
                self.assertIn('MCP Server', mock_stdout.getvalue())
                self.assertNotIn('sample_server_1', mock_stdout.getvalue())

            # Test add command
            with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
                with patch('sys.argv', ['install.py', 'add', 'any_server']):
                    install.main()
                self.assertIn('not found', mock_stderr.getvalue())


if __name__ == '__main__':
    unittest.main()

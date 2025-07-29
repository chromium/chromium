#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for install.py."""

import unittest
import os
import shutil
import tempfile
import io
from pathlib import Path
from unittest.mock import patch, MagicMock

import install


class InstallTest(unittest.TestCase):
    """Tests for the MCP server installation script."""

    def setUp(self):
        """Sets up the test environment."""
        self.tmpdir = tempfile.mkdtemp()
        self.mcp_dir = Path(self.tmpdir) / 'agents' / 'mcp'
        self.mcp_dir.mkdir(parents=True)
        self.extension_dir = Path(self.tmpdir) / '.gemini' / 'extensions'
        self.extension_dir.mkdir(parents=True)
        self.global_extension_dir = Path(
            self.tmpdir) / 'home' / '.gemini' / 'extensions'
        self.global_extension_dir.mkdir(parents=True)

        # Create sample servers
        self.server1_dir = self.mcp_dir / 'sample_server_1'
        self.server1_dir.mkdir()
        with open(self.server1_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_1", "version": "1.0.0"}')
        with open(self.server1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("hello")')

        self.server2_dir = self.mcp_dir / 'sample_server_2'
        self.server2_dir.mkdir()
        with open(self.server2_dir / 'gemini-extension.json',
                  'w',
                  encoding='utf-8') as f:
            f.write('{"name": "sample_server_2", "version": "2.0.0"}')

    def tearDown(self):
        """Tears down the test environment."""
        shutil.rmtree(self.tmpdir)

    def test_get_dir_hash(self):
        """Tests the get_dir_hash function."""
        hash1 = install.get_dir_hash(self.server1_dir)
        hash2 = install.get_dir_hash(self.server1_dir)
        self.assertEqual(hash1, hash2)

        # Test that a change in content changes the hash
        with open(self.server1_dir / 'main.py', 'w', encoding='utf-8') as f:
            f.write('print("world")')
        hash3 = install.get_dir_hash(self.server1_dir)
        self.assertNotEqual(hash1, hash3)

    @patch('subprocess.check_output', side_effect=FileNotFoundError)
    def test_get_dir_hash_fallback(self, mock_check_output):
        """Tests the get_dir_hash function's fallback mechanism."""
        hash1 = install.get_dir_hash(self.server1_dir)
        hash2 = install.get_dir_hash(self.server1_dir)
        self.assertEqual(hash1, hash2)
        self.assertIsNotNone(hash1)

    @patch('install.get_dir_hash')
    def test_is_up_to_date(self, mock_get_dir_hash):
        """Tests the is_up_to_date() function."""
        mock_get_dir_hash.return_value = b'some_hash'
        self.assertFalse(
            install.is_up_to_date('sample_server_1', self.mcp_dir,
                                  self.extension_dir))

        install.add_server('sample_server_1', self.mcp_dir, self.extension_dir)
        mock_get_dir_hash.side_effect = [b'some_hash', b'some_hash']
        self.assertTrue(
            install.is_up_to_date('sample_server_1', self.mcp_dir,
                                  self.extension_dir))

        mock_get_dir_hash.side_effect = [b'new_hash', b'old_hash']
        self.assertFalse(
            install.is_up_to_date('sample_server_1', self.mcp_dir,
                                  self.extension_dir))

    @patch('builtins.input', return_value='y')
    def test_add_server(self, mock_input):
        """Tests the add_server function."""
        install.add_server('sample_server_1', self.mcp_dir, self.extension_dir)
        self.assertTrue((self.extension_dir / 'sample_server_1').exists())

    @patch('builtins.input', return_value='n')
    @patch('install.is_up_to_date', return_value=False)
    def test_add_server_decline_update(self, mock_is_up_to_date, mock_input):
        """Tests that adding an existing server is skipped if the user
        declines."""
        install.add_server('sample_server_1', self.mcp_dir, self.extension_dir)
        with patch('shutil.copytree') as mock_copy:
            install.add_server('sample_server_1', self.mcp_dir,
                               self.extension_dir)
            mock_copy.assert_not_called()

    def test_update_server(self):
        """Tests the update_server function."""
        # Test updating a non-existent server
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            install.update_server('sample_server_1', self.mcp_dir,
                                  self.extension_dir)
            self.assertIn('not installed', mock_stderr.getvalue())

        # Test updating an up-to-date server
        install.add_server('sample_server_1', self.mcp_dir, self.extension_dir)
        with patch('install.is_up_to_date', return_value=True):
            with patch('sys.stdout', new_callable=io.StringIO) as mock_stdout:
                install.update_server('sample_server_1', self.mcp_dir,
                                      self.extension_dir)
                self.assertIn('already up to date', mock_stdout.getvalue())

    def test_remove_server_not_installed(self):
        """Tests removing a server that is not installed."""
        with patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
            install.remove_server('sample_server_1', self.extension_dir)
            self.assertIn('not found', mock_stderr.getvalue())

    @patch('install.get_extension_dir')
    @patch('install.add_server')
    @patch('pathlib.Path.resolve')
    def test_main_add_global(self, mock_resolve, mock_add_server,
                             mock_get_extension_dir):
        """Tests the main function with the add command and --global flag."""
        mock_resolve.return_value = self.mcp_dir
        mock_get_extension_dir.return_value = self.global_extension_dir
        with patch('sys.argv', ['install.py', 'add', '-g', 'sample_server_1']):
            install.main()
        mock_add_server.assert_called_once_with('sample_server_1',
                                                self.mcp_dir,
                                                self.global_extension_dir)

    @patch('install.update_server')
    @patch('install.get_installed_servers', return_value=['sample_server_1'])
    @patch('pathlib.Path.resolve')
    def test_main_update_all(self, mock_resolve, mock_get_installed,
                             mock_update_server):
        """Tests the main function with the update command and no servers."""
        mock_resolve.return_value = self.mcp_dir
        with patch('sys.argv', ['install.py', 'update']):
            install.main()
            mock_update_server.assert_called_once()

    @patch('sys.stderr', new_callable=io.StringIO)
    @patch('pathlib.Path.resolve')
    def test_main_invalid_server(self, mock_resolve, mock_stderr):
        """Tests that main handles invalid server names gracefully."""
        mock_resolve.return_value = self.mcp_dir
        with patch('sys.argv', ['install.py', 'add', 'invalid_server']):
            install.main()
            self.assertIn("Error: Server 'invalid_server' not found",
                          mock_stderr.getvalue())


if __name__ == '__main__':
    unittest.main()

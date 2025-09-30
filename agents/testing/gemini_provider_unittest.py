#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_provider."""

import pathlib
import unittest
import unittest.mock

import gemini_provider


class GeminiProviderUnittest(unittest.TestCase):
    """Unit tests for the `gemini_provider` module."""

    def setUp(self):
        popen_patcher = unittest.mock.patch('subprocess.Popen')
        self.mock_popen = popen_patcher.start()
        self.addCleanup(popen_patcher.stop)

        check_call_patcher = unittest.mock.patch('subprocess.check_call')
        self.mock_check_call = check_call_patcher.start()
        self.addCleanup(check_call_patcher.stop)

        mock_process = unittest.mock.MagicMock()
        mock_process.stdin = unittest.mock.MagicMock()
        mock_process.stdout.readline.side_effect = ['test output\n', '']
        mock_process.wait.return_value = 0
        mock_process.returncode = 0
        self.mock_popen.return_value = mock_process

    def test_call_api_no_gemini_cli_bin(self):
        """Tests that the default command is used when no bin is provided."""
        options = {'config': {}}
        context = {'vars': {}}

        gemini_provider.call_api('test prompt', options, context)

        self.mock_popen.assert_called_once()
        popen_args = self.mock_popen.call_args[0][0]
        self.assertEqual(popen_args, ['gemini', '-y'])

    def test_call_api_with_gemini_cli_bin(self):
        """Tests that a custom command is used when a bin is provided."""
        options = {'config': {}}
        gemini_cli_bin = pathlib.Path('/', 'custom', 'gemini')
        context = {'vars': {'gemini_cli_bin': str(gemini_cli_bin)}}

        gemini_provider.call_api('test prompt', options, context)

        self.mock_popen.assert_called_once()
        popen_args = self.mock_popen.call_args[0][0]
        self.assertEqual(popen_args, [str(gemini_cli_bin), '-y'])


if __name__ == '__main__':
    unittest.main()

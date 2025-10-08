#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_helpers."""

import subprocess
import unittest
import unittest.mock

import gemini_helpers


class GetGeminiExecutableUnittest(unittest.TestCase):
    """Unit tests for the `get_gemini_executable` function."""

    def setUp(self):
        """Sets up the mocks for the tests."""
        which_patcher = unittest.mock.patch('shutil.which')
        self.mock_which = which_patcher.start()
        self.addCleanup(which_patcher.stop)

        exists_patcher = unittest.mock.patch('pathlib.Path.exists')
        self.mock_exists = exists_patcher.start()
        self.addCleanup(exists_patcher.stop)

    def test_get_gemini_executable_from_path(self):
        """Tests that the gemini executable is found in the PATH."""
        self.mock_which.return_value = '/usr/bin/gemini'
        self.assertEqual(gemini_helpers.get_gemini_executable(),
                         '/usr/bin/gemini')

    def test_get_gemini_executable_from_fallback(self):
        """Tests finding the gemini executable in the fallback path."""
        self.mock_which.return_value = None
        self.mock_exists.return_value = True
        self.assertEqual(gemini_helpers.get_gemini_executable(),
                         '/google/bin/releases/gemini-cli/tools/gemini')

    def test_get_gemini_executable_not_found(self):
        """Tests the default gemini command is returned when not found."""
        self.mock_which.return_value = None
        self.mock_exists.return_value = False
        self.assertEqual(gemini_helpers.get_gemini_executable(), 'gemini')


class GetGeminiVersionUnittest(unittest.TestCase):
    """Unit tests for the `get_gemini_version` function."""

    def setUp(self):
        """Sets up the mocks for the tests."""
        run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def test_get_gemini_version_succeeds(self):
        """Tests that the gemini version is correctly parsed."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='gemini version 0.1.2\n', returncode=0)
        self.assertEqual(gemini_helpers.get_gemini_version(), '0.1.2')

    def test_get_gemini_version_fails_on_error(self):
        """Tests that None is returned when `gemini --version` fails."""
        self.mock_run.side_effect = subprocess.CalledProcessError(1, 'gemini')
        self.assertIsNone(gemini_helpers.get_gemini_version())

    def test_get_gemini_version_fails_on_not_found(self):
        """Tests that None is returned when the gemini command is not found."""
        self.mock_run.side_effect = FileNotFoundError
        self.assertIsNone(gemini_helpers.get_gemini_version())

    def test_get_gemini_version_fails_on_no_match(self):
        """Tests for None when the version is not in the output."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='some other output\n', returncode=0)
        self.assertIsNone(gemini_helpers.get_gemini_version())


if __name__ == '__main__':
    unittest.main()

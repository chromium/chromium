#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_helpers."""

import os
import subprocess
import sys
import unittest
import unittest.mock
from pathlib import Path

import gemini_helpers


class PowerShellPathContextUnittest(unittest.TestCase):
    """Unit tests for the `powershell_path_context` context manager."""

    @unittest.skipIf(sys.platform != 'win32', 'Windows only test')
    def test_powershell_path_context_adds_ps1(self):
        """Tests that .PS1 is added to PATHEXT on Windows."""
        with unittest.mock.patch.dict(os.environ,
                                      {'PATHEXT': '.COM;.EXE;.BAT'}):
            with gemini_helpers.powershell_path_context():
                self.assertEqual(os.environ['PATHEXT'], '.COM;.EXE;.BAT;.PS1')
            self.assertEqual(os.environ['PATHEXT'], '.COM;.EXE;.BAT')

    @unittest.skipIf(sys.platform != 'win32', 'Windows only test')
    def test_powershell_path_context_prefers_ps1_over_js(self):
        """Tests that .PS1 is inserted before .JS if .JS exists."""
        # Case 1: .JS is uppercase
        with unittest.mock.patch.dict(os.environ,
                                      {'PATHEXT': '.EXE;.JS;.CMD'}):
            with gemini_helpers.powershell_path_context():
                pathext = os.environ['PATHEXT']
                self.assertIn('.PS1', pathext.upper())
                self.assertLess(pathext.upper().find('.PS1'),
                                pathext.upper().find('.JS'))

        # Case 2: .js is lowercase (testing case sensitivity fix)
        with unittest.mock.patch.dict(os.environ,
                                      {'PATHEXT': '.exe;.js;.cmd'}):
            with gemini_helpers.powershell_path_context():
                pathext = os.environ['PATHEXT']
                self.assertIn('.PS1', pathext.upper())
                # Check for original case preservation
                self.assertIn('.exe', pathext)
                self.assertIn('.cmd', pathext)
                self.assertIn('.js', pathext)
                # Ensure .PS1 is before .js
                self.assertLess(pathext.upper().find('.PS1'),
                                pathext.upper().find('.JS'))

    @unittest.skipIf(sys.platform != 'win32', 'Windows only test')
    def test_powershell_path_context_does_nothing_if_ps1_exists(self):
        """Tests that PATHEXT is unchanged if .PS1 is already there."""
        with unittest.mock.patch.dict(os.environ,
                                      {'PATHEXT': '.EXE;.PS1;.BAT'}):
            with gemini_helpers.powershell_path_context():
                self.assertEqual(os.environ['PATHEXT'], '.EXE;.PS1;.BAT')


class GetGeminiCommandUnittest(unittest.TestCase):
    """Unit tests for the `get_gemini_command` function."""

    def setUp(self):
        """Sets up the mocks for the tests."""
        gemini_helpers.get_gemini_command.cache_clear()
        which_patcher = unittest.mock.patch('shutil.which')
        self.mock_which = which_patcher.start()
        self.addCleanup(which_patcher.stop)

        exists_patcher = unittest.mock.patch('pathlib.Path.exists')
        self.mock_exists = exists_patcher.start()
        self.addCleanup(exists_patcher.stop)

        run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = run_patcher.start()
        self.addCleanup(run_patcher.stop)

    def test_get_gemini_command_from_path_preference(self):
        """Tests that PATH is preferred over binfs."""
        self.mock_which.return_value = '/usr/bin/gemini'
        self.mock_exists.return_value = True
        # which is checked first, so it should return /usr/bin/gemini
        result = gemini_helpers.get_gemini_command()
        self.assertEqual(result, ['/usr/bin/gemini'])

    def test_get_gemini_command_from_binfs(self):
        """Tests that binfs path is used if not in PATH."""
        self.mock_which.return_value = None
        self.mock_exists.return_value = True
        result = gemini_helpers.get_gemini_command()
        # On Windows str(Path('/google/bin/...')) uses backslashes
        expected = str(Path('/google/bin/releases/gemini-cli/tools/gemini'))
        self.assertEqual(result, [expected])

    def test_get_gemini_command_from_path(self):
        """Tests that the gemini command is found in the PATH."""
        self.mock_exists.return_value = False
        self.mock_which.return_value = '/usr/bin/gemini'
        self.assertEqual(gemini_helpers.get_gemini_command(),
                         ['/usr/bin/gemini'])

    def test_get_gemini_command_not_found(self):
        """Tests the default gemini command is returned when not found."""
        self.mock_which.return_value = None
        self.mock_exists.return_value = False
        self.assertEqual(gemini_helpers.get_gemini_command(), ['gemini'])

    @unittest.skipIf(sys.platform == 'win32', 'Unix only test')
    def test_get_gemini_command_from_alias(self):
        """Tests that the gemini command is found from a shell alias."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='alias gemini=\'/custom/path/gemini\'\n', returncode=0)
        self.assertEqual(gemini_helpers.get_gemini_command(use_alias=True),
                         ['/custom/path/gemini'])

    @unittest.skipIf(sys.platform == 'win32', 'Unix only test')
    def test_get_gemini_command_from_alias_zsh_style(self):
        """Tests that the gemini command is found from a zsh alias."""
        # zsh style: gemini=/path/to/exe
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='gemini=/custom/path/gemini\n', returncode=0)
        self.assertEqual(gemini_helpers.get_gemini_command(use_alias=True),
                         ['/custom/path/gemini'])

    @unittest.skipIf(sys.platform == 'win32', 'Unix only test')
    def test_get_gemini_command_from_alias_zsh_style_quoted(self):
        """Tests that the gemini command is found from a quoted zsh alias."""
        # zsh style quoted: gemini='/path/to/exe'
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='gemini=\'/custom/path/gemini\'\n', returncode=0)
        self.assertEqual(gemini_helpers.get_gemini_command(use_alias=True),
                         ['/custom/path/gemini'])

    def test_get_gemini_command_ignores_alias_if_not_requested(self):
        """Tests that aliases are ignored if `use_alias` is False."""
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout='alias gemini=\'/custom/path/gemini\'\n', returncode=0)
        self.mock_which.return_value = '/usr/bin/gemini'
        self.assertEqual(gemini_helpers.get_gemini_command(use_alias=False),
                         ['/usr/bin/gemini'])

    def test_get_gemini_command_handles_alias_failure(self):
        """Tests fallback if alias command fails or is not found."""
        # Mock alias command failing
        self.mock_run.return_value = unittest.mock.MagicMock(returncode=1)
        self.mock_which.return_value = '/usr/bin/gemini'
        self.assertEqual(gemini_helpers.get_gemini_command(use_alias=True),
                         ['/usr/bin/gemini'])

class GetGeminiVersionUnittest(unittest.TestCase):
    """Unit tests for the `get_gemini_version` function."""

    def setUp(self):
        """Sets up the mocks for the tests."""
        gemini_helpers.get_gemini_version.cache_clear()
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

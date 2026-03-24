# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for run.py"""

import unittest
import os
from unittest import mock

# Add the script's directory to the Python path to allow importing it.
import sys
sys.path.append(os.path.join(os.path.dirname(__file__)))

import run

class RunScriptTest(unittest.TestCase):

    @mock.patch('builtins.open',
                new_callable=mock.mock_open,
                read_data='initial content')
    @mock.patch('os.path.exists', return_value=True)
    def test_setup_gemini_context_md(self, _mock_exists, mock_open):
        """Tests the setup_gemini_context_md context manager."""
        context_files = ['file1.md', 'file2.cc']
        entry = (f"# {run.SPANIFICATION_GEMINI_MD}\n"
                 f"@file1.md\n@file2.cc\n# /{run.SPANIFICATION_GEMINI_MD}\n")

        with run.setup_gemini_context_md(context_files):
            # Check if the entry was added
            mock_open.assert_called_with(run.GEMINI_MD_PATH,
                                         'w',
                                         encoding='utf-8')
            mock_open().write.assert_called_with('initial content' + entry)

        # Check if the entry was removed
        mock_open.assert_called_with(run.GEMINI_MD_PATH,
                                     'w',
                                     encoding='utf-8')
        mock_open().write.assert_called_with('initial content')

    @mock.patch('subprocess.run')
    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('builtins.open', new_callable=mock.mock_open)
    def test_ensure_gn_build_dir(self, _mock_open, _mock_exists, mock_run):
        """Tests ensure_gn_build_dir always runs gn gen."""
        run.ensure_gn_build_dir()
        # Verify gn gen is called for each platform
        expected_calls = [
            mock.call(['gn', 'gen', os.path.join('out', d)], check=True)
            for d in run.REQUIRED_BUILD_DIRS
        ]
        mock_run.assert_has_calls(expected_calls, any_order=True)

    @mock.patch('subprocess.run')
    def test_run_deterministic_check_success(self, mock_run):
        """Tests run_deterministic_check with successful compilation/tests."""
        mock_run.return_value.returncode = 0
        errors = run.run_deterministic_check('file.cc')
        self.assertEqual(len(errors), 0)
        # Should call autoninja for each platform + autotest.py
        self.assertEqual(mock_run.call_count, len(run.REQUIRED_BUILD_DIRS) + 1)

    @mock.patch('subprocess.run')
    def test_run_deterministic_check_failure(self, mock_run):
        """Tests run_deterministic_check with compilation failure."""
        mock_run.return_value.returncode = 1
        mock_run.return_value.stderr = "Error message"
        errors = run.run_deterministic_check('file.cc')
        self.assertGreater(len(errors), 0)
        self.assertIn("Error message", errors[0])

    @mock.patch('run.run_gemini')
    @mock.patch('subprocess.run')
    @mock.patch('builtins.open',
                new_callable=mock.mock_open,
                read_data='patch content')
    def test_run_reviewer_success(self, _mock_open, mock_run, mock_gemini):
        """Tests run_reviewer with a successful review."""
        mock_run.return_value.stdout = "patch content"
        mock_gemini.return_value = (0, [{'output': 'LGTM'}])
        status, feedback = run.run_reviewer('file.cc')
        self.assertEqual(status, "SUCCESS")
        self.assertIn("LGTM", feedback)

    @mock.patch('run.run_gemini')
    @mock.patch('subprocess.run')
    @mock.patch('builtins.open',
                new_callable=mock.mock_open,
                read_data='patch content')
    def test_run_reviewer_failure(self, _mock_open, mock_run, mock_gemini):
        """Tests run_reviewer with changes requested."""
        mock_run.return_value.stdout = "patch content"
        mock_gemini.return_value = (0,
                                    [{'output': 'CHANGES_REQUESTED: fix this'}])
        status, feedback = run.run_reviewer('file.cc')
        self.assertEqual(status, "FAILURE")
        self.assertIn("CHANGES_REQUESTED", feedback)

if __name__ == '__main__':
    unittest.main()

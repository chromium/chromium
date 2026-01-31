#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for auto_fixer."""

import os
import subprocess
import sys
import unittest
import unittest.mock

# Add the Chromium root to sys.path to allow for fully qualified imports.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))

from agents.projects.modernization import auto_fixer


class AutoFixerUnittest(unittest.TestCase):
    """Unit tests for the auto_fixer module."""

    def setUp(self):
        self.mock_run_patcher = unittest.mock.patch('subprocess.run')
        self.mock_run = self.mock_run_patcher.start()

        self.mock_get_exe_patcher = unittest.mock.patch(
            'agents.common.gemini_helpers.get_gemini_executable')
        self.mock_get_exe = self.mock_get_exe_patcher.start()
        self.mock_get_exe.return_value = '/path/to/gemini'

        self.verify_cmd = ['run_tests']

        self.addCleanup(self.mock_run_patcher.stop)
        self.addCleanup(self.mock_get_exe_patcher.stop)

    def test_fix_success_no_verification(self):
        """Test that fix calls gemini with -y and returns SUCCESS without
        verification."""
        # Mock subprocess.run return value for success
        mock_ret = unittest.mock.MagicMock()
        mock_ret.returncode = 0
        mock_ret.stdout = 'Fixed it!'
        self.mock_run.return_value = mock_ret

        error_msg = 'Some error occurred'
        fixer = auto_fixer.AutoFixer()
        result = fixer.fix(error_msg)

        self.assertEqual(result, auto_fixer.FixStatus.SUCCESS)
        # Only Gemini call
        self.assertEqual(self.mock_run.call_count, 1)

        # Verify call arguments
        cmd_args = self.mock_run.call_args[0][0]
        kwargs = self.mock_run.call_args[1]

        self.assertEqual(cmd_args[0], '/path/to/gemini')
        self.assertIn('-y', cmd_args)
        self.assertIn(error_msg, kwargs['input'])
        self.assertEqual(kwargs['timeout'], 300)

    def test_fix_success_with_verification(self):
        """Test that fix calls gemini and returns SUCCESS after verification."""
        # Mock subprocess.run return value for success
        mock_ret = unittest.mock.MagicMock()
        mock_ret.returncode = 0
        mock_ret.stdout = 'Fixed it!'
        self.mock_run.side_effect = [mock_ret, mock_ret]

        fixer = auto_fixer.AutoFixer(verification_timeout=123)
        result = fixer.fix('Some error occurred',
                           verification_command=self.verify_cmd)

        self.assertEqual(result, auto_fixer.FixStatus.SUCCESS)
        # Gemini call + Verification call
        self.assertEqual(self.mock_run.call_count, 2)

        # Check timeout for verification call
        kwargs = self.mock_run.call_args_list[1][1]
        self.assertEqual(kwargs['timeout'], 123)

    def test_fix_prompt_updated_on_retry(self):
        """Test that the prompt includes failure output on retry."""
        failure_output = 'First failure'
        initial_error = 'Initial error'
        self.mock_run.side_effect = [
            # Gemini fails
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=1,
                                        stdout=failure_output),
            # Gemini succeeds
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=0,
                                        stdout='Success'),
        ]

        fixer = auto_fixer.AutoFixer(max_attempts=2)
        fixer.fix(initial_error)

        self.assertEqual(self.mock_run.call_count, 2)

        # Verify second call prompt includes 'First failure'
        kwargs = self.mock_run.call_args_list[1][1]
        self.assertIn(failure_output, kwargs['input'])
        self.assertNotIn(initial_error,
                         kwargs['input'])  # Replaced by failure_output

    def test_fix_failure(self):
        """Test that fix returns AGENT_FAILURE when gemini fails."""
        # Simulate failure by returning non-zero return code
        self.mock_run.return_value = subprocess.CompletedProcess(
            args=['gemini'], returncode=1, stdout='Error')

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)

        self.assertEqual(result, auto_fixer.FixStatus.AGENT_FAILURE)
        # Default attempts is 3
        self.assertEqual(self.mock_run.call_count, 3)

    def test_fix_timeout(self):
        """Test that fix retries on timeout and fails with AGENT_FAILURE."""
        # run_command catches TimeoutExpired and returns CompletedProcess with
        # TIMEOUT_EXIT_CODE.
        self.mock_run.side_effect = subprocess.TimeoutExpired(cmd=['gemini'],
                                                              timeout=300)

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)
        self.assertEqual(result, auto_fixer.FixStatus.AGENT_FAILURE)
        # Default attempts is 3
        self.assertEqual(self.mock_run.call_count, 3)

    def test_fix_retry_success(self):
        """Test that fix retries and succeeds."""
        # Fail twice, then succeed
        success_ret = unittest.mock.MagicMock()
        success_ret.returncode = 0
        success_ret.stdout = 'Fixed'

        # Mocking run_command's behavior:
        # TimeoutExpired -> catch and return CompletedProcess

        # We need 3 calls to gemini and 1 call to verification
        self.mock_run.side_effect = [
            # Timeout on first Gemini call
            subprocess.TimeoutExpired(cmd=['gemini'], timeout=300),
            # Failure on second Gemini call
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=1,
                                        stdout='Fail'),
            # Success on third Gemini call
            success_ret,
            # Success on verification
            success_ret,
        ]

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)
        self.assertEqual(result, auto_fixer.FixStatus.SUCCESS)
        self.assertEqual(self.mock_run.call_count, 4)

    def test_fix_verification_command_success(self):
        """Test that fix runs verification command and returns SUCCESS."""
        self.mock_run.side_effect = [
            # Gemini succeeds
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=0,
                                        stdout='Agent finished'),
            # Verification succeeds
            subprocess.CompletedProcess(args=self.verify_cmd,
                                        returncode=0,
                                        stdout='Tests passed')
        ]

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)

        self.assertEqual(result, auto_fixer.FixStatus.SUCCESS)
        self.assertEqual(self.mock_run.call_count, 2)
        # Verify verification command was called correctly
        self.assertEqual(self.mock_run.call_args_list[1][0][0],
                         self.verify_cmd)

    def test_fix_verification_command_failure_then_success(self):
        """Test that fix retries if verification fails."""
        verify_failure = 'Tests failed 1'
        self.mock_run.side_effect = [
            # Agent success
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=0,
                                        stdout='Agent finished 1'),
            # Verification failure
            subprocess.CompletedProcess(args=self.verify_cmd,
                                        returncode=1,
                                        stdout=verify_failure),
            # Agent success
            subprocess.CompletedProcess(args=['gemini'],
                                        returncode=0,
                                        stdout='Agent finished 2'),
            # Verification success
            subprocess.CompletedProcess(args=self.verify_cmd,
                                        returncode=0,
                                        stdout='Tests passed 2')
        ]

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)

        self.assertEqual(result, auto_fixer.FixStatus.SUCCESS)
        self.assertEqual(self.mock_run.call_count, 4)

        # Verify that the second Gemini call's prompt contains the first
        # verification failure
        second_gemini_call_kwargs = self.mock_run.call_args_list[2][1]
        self.assertIn(verify_failure, second_gemini_call_kwargs['input'])

    def test_fix_verification_command_all_failures(self):
        """Test that fix returns VERIFICATION_FAILURE if verification fails in
        all attempts."""
        # Agent always succeeds, but verification always fails
        self.mock_run.side_effect = [
            subprocess.CompletedProcess(
                args=['gemini'], returncode=0, stdout='Agent finished'),
            subprocess.CompletedProcess(
                args=self.verify_cmd, returncode=1, stdout='Tests failed')
        ] * 3

        fixer = auto_fixer.AutoFixer()
        result = fixer.fix('Error', verification_command=self.verify_cmd)

        self.assertEqual(result, auto_fixer.FixStatus.VERIFICATION_FAILURE)
        # Default attempts is 3, each attempt runs gemini + verification
        self.assertEqual(self.mock_run.call_count, 6)


if __name__ == '__main__':
    unittest.main()

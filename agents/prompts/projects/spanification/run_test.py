# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for run.py"""

import unittest
import subprocess
import json
import os
from unittest import mock

# Add the script's directory to the Python path to allow importing it.
import sys
sys.path.append(os.path.join(os.path.dirname(__file__)))

import run

class RunScriptTest(unittest.TestCase):

    @mock.patch('builtins.open',
                new_callable=mock.mock_open, read_data='initial content')
    @mock.patch('os.path.exists', return_value=True)
    def test_setup_gemini_context_md(self, _mock_exists, mock_open):
        """Tests the setup_gemini_context_md context manager."""
        prompt_path = 'prompts/test_prompt.md'
        entry = (f"# {run.SPANIFICATION_GEMINI_MD}\n"
                 f"@{prompt_path}\n# /{run.SPANIFICATION_GEMINI_MD}\n")

        with run.setup_gemini_context_md(prompt_path):
            # Check if the entry was added
            mock_open.assert_called_with(run.GEMINI_MD_PATH,
                                         'w',
                                         encoding='utf-8')
            mock_open().write.assert_called_with('initial content' + entry)

        # Check if the entry was removed
        mock_open.assert_called_with(run.GEMINI_MD_PATH, 'w', encoding='utf-8')
        mock_open().write.assert_called_with('initial content')


    @mock.patch('subprocess.Popen')
    @mock.patch('os.path.exists')
    @mock.patch('os.makedirs')
    @mock.patch('subprocess.run')
    @mock.patch('builtins.open', new_callable=mock.mock_open)
    def test_run_gemini_success(self, mock_open, mock_run, mock_makedirs,
                                mock_exists, mock_subprocess_popen):
        """Tests a successful run_gemini execution."""
        mock_exists.return_value = True
        mock_process = mock.Mock()
        mock_process.stdout.readline.side_effect = ['{"output": "line"}\n', '']
        mock_process.wait.return_value = 0
        mock_subprocess_popen.return_value.__enter__.return_value = mock_process

        summary_data = {'status': 'SUCCESS', 'summary': 'It worked!'}

        # Simulate reading summary.json
        def open_side_effect(path, _mode='r', encoding=None):
            assert encoding == 'utf-8'
            if path == run.SUMMARY_PATH:
                m = mock.mock_open(read_data=json.dumps(summary_data))
                return m()
            m = mock.mock_open()
            return m()
        mock_open.side_effect = open_side_effect

        result = run.run_gemini('file.cc')

        self.assertEqual(result['status'], 'SUCCESS')
        self.assertEqual(result['summary'], summary_data)
        self.assertEqual(result['exit_code'], 0)
        mock_subprocess_popen.assert_called_once()
        mock_makedirs.assert_called_with(run.GEMINI_OUT_DIR, exist_ok=True)
        mock_run.assert_any_call(['rm', '-rf', run.GEMINI_OUT_DIR], check=True)
        cmd = mock_subprocess_popen.call_args[0][0]
        self.assertIn('gemini', cmd)
        self.assertIn('--output-format', cmd)
        self.assertIn('stream-json', cmd)

    @mock.patch('subprocess.Popen')
    @mock.patch('os.path.exists', return_value=False)
    @mock.patch('os.makedirs')
    @mock.patch('subprocess.run')
    def test_run_gemini_timeout(self, _mock_run, _mock_makedirs, _mock_exists,
                                mock_subprocess_popen):
        """Tests run_gemini with a timeout."""
        mock_process = mock.Mock()
        mock_process.stdout.readline.side_effect = ['']
        mock_process.wait.side_effect = subprocess.TimeoutExpired(cmd='gemini',
                                                                  timeout=10)
        mock_process.kill.return_value = None
        mock_subprocess_popen.return_value.__enter__.return_value = mock_process

        result = run.run_gemini('file.cc')
        self.assertEqual(result['status'], 'TIMEOUT')
        self.assertEqual(result['exit_code'], 124)


if __name__ == '__main__':
    unittest.main()

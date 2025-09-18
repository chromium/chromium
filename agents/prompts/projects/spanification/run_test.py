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

    @mock.patch('subprocess.run')
    def test_discover_unsafe_todos_found(self, mock_subprocess_run):
        """Tests discover_unsafe_todos when files are found."""
        mock_result = mock.Mock()
        mock_result.returncode = 0
        mock_result.stdout = 'file1.cc\nfile2.cc\n'
        mock_subprocess_run.return_value = mock_result

        files = run.discover_unsafe_todos()
        self.assertEqual(files, ['file1.cc', 'file2.cc'])
        mock_subprocess_run.assert_called_with([
            'git', 'grep', '-l', '-e', 'UNSAFE_TODO', '--or', '-e',
            'allow_unsafe_buffers'
        ],
                                               capture_output=True,
                                               text=True)

    @mock.patch('subprocess.run')
    def test_discover_unsafe_todos_not_found(self, mock_subprocess_run):
        """Tests discover_unsafe_todos when no files are found."""
        mock_result = mock.Mock()
        mock_result.returncode = 1
        mock_result.stdout = ''
        mock_subprocess_run.return_value = mock_result

        files = run.discover_unsafe_todos()
        self.assertEqual(files, [])

    @mock.patch('subprocess.run')
    def test_discover_unsafe_todos_with_folder(self, mock_subprocess_run):
        """Tests discover_unsafe_todos when a folder is specified."""
        mock_result = mock.Mock()
        mock_result.returncode = 0
        mock_result.stdout = 'test/file1.cc\n'
        mock_subprocess_run.return_value = mock_result

        files = run.discover_unsafe_todos(folder='test/')
        self.assertEqual(files, ['test/file1.cc'])
        self.assertIn('test/', mock_subprocess_run.call_args[0][0])

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
            mock_open().write.assert_called_with('initial content' + entry)

        # Check if the entry was removed
        mock_open().write.assert_called_with('initial content')


    @mock.patch('time.time', side_effect=[100.0, 200.0])
    @mock.patch('subprocess.run')
    @mock.patch('os.path.exists')
    @mock.patch('os.makedirs')
    @mock.patch('os.remove')
    @mock.patch('builtins.open', new_callable=mock.mock_open)
    def test_run_gemini_success(self, mock_open, _mock_remove, _mock_makedirs,
                               mock_exists, mock_subprocess_run, _mock_time):
        """Tests a successful run_gemini execution."""
        mock_exists.return_value = True
        mock_subprocess_run.return_value = mock.Mock(returncode=0)
        summary_data = {'status': 'SUCCESS', 'summary': 'It worked!'}
        session_summary_data = {'turns': 5}

        # Simulate reading summary.json and session-summary.json
        def open_side_effect(path, _mode='r'):
            if path == run.SUMMARY_JSON_PATH:
                return mock.mock_open(read_data=json.dumps(summary_data))()
            if path == run.SESSION_SUMMARY_PATH:
                return mock.mock_open(
                    read_data=json.dumps(session_summary_data))()
            return mock.mock_open()()
        mock_open.side_effect = open_side_effect

        result = run.run_gemini('test prompt', ['file.cc'])

        self.assertEqual(result['status'], 'SUCCESS')
        self.assertEqual(result['summary'], 'It worked!')
        self.assertEqual(result['session_summary']['turns'], 5)
        self.assertEqual(result['duration'], 100)
        self.assertEqual(result['exit_code'], 0)
        mock_subprocess_run.assert_called_once()
        cmd = mock_subprocess_run.call_args[0][0]
        self.assertIn('gemini', cmd)
        self.assertIn('test prompt', cmd)
        self.assertNotIn('--yolo', cmd)

    @mock.patch('time.time', side_effect=[100.0, 200.0])
    @mock.patch('subprocess.run',
                side_effect=subprocess.TimeoutExpired(cmd='gemini', timeout=10))
    @mock.patch('os.path.exists', return_value=False)
    @mock.patch('os.makedirs')
    @mock.patch('os.remove')
    def test_run_gemini_timeout(self, _mock_remove, _mock_makedirs,
                                _mock_exists, _mock_subprocess_run, _mock_time):
        """Tests run_gemini with a timeout."""
        result = run.run_gemini('prompt', ['file.cc'])
        self.assertEqual(result['status'], 'TIMEOUT')
        self.assertEqual(result['exit_code'], 124)
        self.assertEqual(result['duration'], 100)

    @mock.patch('run.run_gemini')
    def test_categorize_file(self, mock_run_gemini):
        """Tests the categorize_file function."""
        run.categorize_file('file.cc', yolo=True)
        prompt = mock_run_gemini.call_args.args[0]
        task_args = mock_run_gemini.call_args.kwargs.get('task_args', None)
        yolo_arg = mock_run_gemini.call_args.kwargs.get('yolo', None)

        self.assertIn('detect the unsafe access', prompt)
        self.assertIn('file.cc', prompt)
        self.assertEqual(task_args, ['file.cc'])
        self.assertTrue(yolo_arg)

    @mock.patch('run.run_gemini')
    def test_generate_fix_known_types(self, mock_run_gemini):
        """Tests generate_fix with known variable and access types."""
        run.generate_fix('file.cc', 'Local-Variable', 'operator[]')
        mock_run_gemini.assert_called_once()
        prompt = mock_run_gemini.call_args[0][0]
        self.assertIn('Arrayify the variable', prompt)
        self.assertIn('just remove the `UNSAFE_TODO`', prompt)

    @mock.patch('run.run_gemini')
    def test_generate_fix_unknown_types(self, mock_run_gemini):
        """Tests generate_fix with unknown types."""
        result = run.generate_fix('file.cc', 'Unknown-Type', 'operator[]')
        self.assertEqual(result['status'], 'NOT_SUPPORTED')
        mock_run_gemini.assert_not_called()

    @mock.patch('subprocess.run')
    @mock.patch('os.path.exists')
    def test_autocommit_changes_success(self, mock_exists, mock_subprocess_run):
        """Tests autocommit_changes on a successful fix."""
        mock_exists.return_value = True
        fix_result = {'status': 'SUCCESS'}
        run.autocommit_changes(fix_result, 'file.cc')

        calls = mock_subprocess_run.call_args_list
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0].args[0][0:4], ['git', 'commit', '-a', '-F'])

    @mock.patch('subprocess.run')
    def test_autocommit_changes_failure(self, mock_subprocess_run):
        """Tests autocommit_changes on a failed fix."""
        fix_result = {'status': 'FAILURE'}
        run.autocommit_changes(fix_result, 'file.cc')
        mock_subprocess_run.assert_called_with(
          ['git', 'reset', '--hard', 'HEAD'], check=True)


if __name__ == '__main__':
    unittest.main()

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for utils.py."""

import os
import subprocess
import sys
import unittest
import unittest.mock

# Add the Chromium root to sys.path to allow for fully qualified imports.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))

from agents.projects.modernization import utils


class TestRunCommand(unittest.TestCase):
    """Tests for run_command function."""

    @unittest.mock.patch('subprocess.run')
    def test_failure(self, mock_subprocess_run):
        """Test failed command execution."""
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=['ls'], returncode=1, stdout='error', stderr='error')

        result = utils.run_command(['ls'], capture_output=True)

        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, 'error')

    @unittest.mock.patch('subprocess.run')
    def test_kwargs(self, mock_subprocess_run):
        """Test passing additional arguments to subprocess.run."""
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=['cmd'], returncode=0, stdout='')

        utils.run_command(['cmd'], input='input', timeout=10)

        mock_subprocess_run.assert_called_with(['cmd'],
                                               stdout=None,
                                               stderr=None,
                                               text=True,
                                               check=False,
                                               input='input',
                                               timeout=10)

    @unittest.mock.patch('subprocess.run')
    def test_success(self, mock_subprocess_run):
        """Test successful command execution."""
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=['ls'], returncode=0, stdout='file1\nfile2', stderr='')

        result = utils.run_command(['ls'], capture_output=True)

        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, 'file1\nfile2')

    @unittest.mock.patch('subprocess.run')
    def test_success_no_capture(self, mock_subprocess_run):
        """Test that output is None when not captured."""
        mock_subprocess_run.return_value = subprocess.CompletedProcess(
            args=['ls'], returncode=0, stdout=None)

        result = utils.run_command(['ls'], capture_output=False)

        self.assertEqual(result.returncode, 0)
        self.assertIsNone(result.stdout)

    @unittest.mock.patch('subprocess.run')
    def test_timeout(self, mock_subprocess_run):
        """Test timeout handling."""
        mock_subprocess_run.side_effect = subprocess.TimeoutExpired(
            cmd=['cmd'], timeout=10, output='partial output')

        result = utils.run_command(['cmd'], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, utils.TIMEOUT_EXIT_CODE)
        self.assertEqual(result.stdout, 'partial output')


class TestTask(unittest.TestCase):
    """Tests for Task class."""

    def test_from_dict(self):
        """Test Task.from_dict method."""
        data = {
            'task_id': 't1',
            'owners_directory': 'dir',
            'files': ['f1'],
            'task_type': 'null_to_nullptr',
            'cl_link': 'link',
            'local_branch': 'branch'
        }
        task = utils.Task.from_dict(data)
        self.assertEqual(task.task_id, 't1')
        self.assertEqual(task.owners_directory, 'dir')
        self.assertEqual(task.files, ['f1'])
        self.assertEqual(task.task_type, utils.TaskType.NULL_TO_NULLPTR)
        self.assertEqual(task.cl_link, 'link')
        self.assertEqual(task.local_branch, 'branch')

    def test_from_dict_default_task_type(self):
        """Test Task.from_dict method with missing task_type."""
        data = {
            'owners_directory': 'dir',
            'files': ['f1'],
        }
        task = utils.Task.from_dict(data)
        self.assertEqual(task.task_type, utils.TaskType.UNDEFINED)
        self.assertIn('undefined', task.task_id)

    def test_to_dict(self):
        """Test Task.to_dict method."""
        task = utils.Task(task_id='t1',
                          owners_directory='dir',
                          files=['f1'],
                          task_type=utils.TaskType.NULL_TO_NULLPTR,
                          cl_link='link',
                          local_branch='branch')
        expected = {
            'task_id': 't1',
            'owners_directory': 'dir',
            'files': ['f1'],
            'task_type': 'null_to_nullptr',
            'cl_link': 'link',
            'local_branch': 'branch'
        }
        self.assertEqual(task.to_dict(), expected)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing serve_repo.py."""

import argparse
import json
import unittest
import unittest.mock as mock

from types import SimpleNamespace

import serve_repo

from common import REPO_ALIAS, register_device_args


_REPO_DIR = 'test_repo_dir'
_REPO_NAME = 'test_repo_name'
_TARGET = 'test_target'
_NO_SERVERS_LIST = json.dumps({'ok': {'data': []}})
_SERVERS_LIST = json.dumps({'ok': {'data': [{'name': _REPO_NAME,
                                            'repo_path': _REPO_DIR}]}})
_WRONG_SERVERS_LIST = json.dumps({'ok': {'data': [{'name': 'wrong_name',
                                            'repo_path': _REPO_DIR}]}})

# Tests private functions.
# pylint: disable=protected-access
class ServeRepoTest(unittest.TestCase):
    """Unittests for serve_repo.py."""

    @mock.patch('serve_repo.run_ffx_command')
    def test_start_server(self, mock_ffx) -> None:
        """Test |_start_serving| function for start."""
        mock_ffx.side_effect =[SimpleNamespace(returncode=0, stdout=''),
                                SimpleNamespace(returncode=0,
                                                stdout=_SERVERS_LIST),
                                SimpleNamespace(returncode=0, stdout='')
                                ]
        serve_repo._start_serving(_REPO_DIR, _REPO_NAME, _TARGET)
        self.assertEqual(mock_ffx.call_count, 3)
        first_call = mock_ffx.call_args_list[0]
        self.assertEqual(mock.call(cmd=['repository', 'server', 'start',
                                        '--background',
                         '--repository', _REPO_NAME, '--repo-path', _REPO_DIR,
                         '--no-device']),
                         first_call)
        second_call = mock_ffx.call_args_list[1]
        self.assertEqual(
            mock.call(cmd=[
                '--machine', 'json', 'repository', 'server', 'list',
                '--name', _REPO_NAME
            ], check=False, capture_output=True), second_call)

        third_call = mock_ffx.call_args_list[2]
        self.assertEqual(
            mock.call(cmd=[
                'target', 'repository', 'register', '-r', _REPO_NAME,
                '--alias', REPO_ALIAS
            ],
                      target_id=_TARGET), third_call)

    @mock.patch('serve_repo.run_ffx_command')
    @mock.patch('serve_repo._assert_server_running')
    def test_start_server_no_start(self,mock_wait, mock_ffx) -> None:
        """Test |_start_serving| function for start."""
        mock_ffx.return_value = SimpleNamespace(returncode=0, stdout='')
        mock_wait.side_effect = RuntimeError(
        'Repository server %s is not running. Output: %s stderr: %s'
        % (_REPO_NAME,'', 'Mock error for not running'))

        with self.assertRaises(RuntimeError):
            serve_repo._start_serving(_REPO_DIR, _REPO_NAME, _TARGET)
        self.assertEqual(mock_ffx.call_count, 1)
        first_call = mock_ffx.call_args_list[0]
        self.assertEqual(mock.call(cmd=['repository', 'server', 'start',
                                        '--background',
                         '--repository', _REPO_NAME, '--repo-path', _REPO_DIR,
                         '--no-device']),
                         first_call)

    @mock.patch('serve_repo.run_ffx_command')
    def test_assert_server_running(self, mock_ffx) -> None:
        """Test |_assert_server_running| function for start."""
        mock_ffx.side_effect =[SimpleNamespace(returncode=0,
                                        stdout=_SERVERS_LIST)]
        # Raises an error if there is a problem, so no need to check for
        # RuntimeError.
        try:
            serve_repo._assert_server_running(_REPO_NAME)
        except RuntimeError as err:
            self.fail(f'Unexpected error: {err}')

    @mock.patch('serve_repo.run_ffx_command')
    def test_is_server_not_running(self, mock_ffx) -> None:
        """Test |_assert_server_running| function for start with no server."""
        mock_ffx.return_value = SimpleNamespace(returncode=0,
                                        stdout=_NO_SERVERS_LIST, stderr='')

        with self.assertRaises(RuntimeError):
            serve_repo._assert_server_running(_REPO_NAME)

    @mock.patch('serve_repo.run_ffx_command')
    def test_is_wrong_server_running(self, mock_ffx) -> None:
        """Test |_assert_server_running| function for start with no server."""
        mock_ffx.return_value = SimpleNamespace(returncode=0,
                                        stdout=_WRONG_SERVERS_LIST, stderr='')

        with self.assertRaises(RuntimeError):
            serve_repo._assert_server_running(_REPO_NAME)

    @mock.patch('serve_repo.run_ffx_command')
    def test_is_server_not_running_bad_ffx(self, mock_ffx) -> None:
        """Test |_assert_server_running| function for start with bad ffx."""
        mock_ffx.return_value = SimpleNamespace(returncode=1,
                                        stderr='Some error', stdout='')

        with self.assertRaises(RuntimeError):
            serve_repo._assert_server_running(_REPO_NAME)

    @mock.patch('serve_repo.run_ffx_command')
    def test_is_server_not_running_bad_json(self, mock_ffx) -> None:
        """Test |_assert_server_running| function for start with bad ffx."""
        mock_ffx.return_value = SimpleNamespace(returncode=0,
                                        stderr='', stdout='{"some": bad...')

        with self.assertRaises(RuntimeError):
            serve_repo._assert_server_running(_REPO_NAME)

    @mock.patch('serve_repo.run_ffx_command')
    def test_stop_server(self, mock_ffx) -> None:
        """Test |_stop_serving| function for stop."""

        serve_repo._stop_serving(_REPO_NAME, _TARGET)
        self.assertEqual(mock_ffx.call_count, 2)
        first_call = mock_ffx.call_args_list[0]
        self.assertEqual(
            mock.call(
                cmd=['target', 'repository', 'deregister', '-r', _REPO_NAME],
                target_id=_TARGET,
                check=False), first_call)
        second_call = mock_ffx.call_args_list[1]
        self.assertEqual(
            mock.call(cmd=['repository', 'server', 'stop', _REPO_NAME],
                      check=False),
            second_call)

    @mock.patch('serve_repo._start_serving')
    @mock.patch('serve_repo._stop_serving')
    def test_serve_repository(self, mock_stop, mock_start) -> None:
        """Tests |serve_repository| context manager."""

        parser = argparse.ArgumentParser()
        serve_repo.register_serve_args(parser)
        register_device_args(parser)
        with serve_repo.serve_repository(
                parser.parse_args([
                    '--repo', _REPO_DIR, '--repo-name', _REPO_NAME,
                    '--target-id', _TARGET
                ])):
            self.assertEqual(mock_start.call_count, 1)
        self.assertEqual(mock_stop.call_count, 1)


if __name__ == '__main__':
    unittest.main()

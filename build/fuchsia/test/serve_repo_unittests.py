#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing serve_repo.py."""

import argparse
import unittest
import unittest.mock as mock

import serve_repo

from common import REPO_ALIAS, register_device_args

_REPO_DIR = 'test_repo_dir'
_REPO_NAME = 'test_repo_name'
_TARGET = 'test_target'


# Tests private functions.
# pylint: disable=protected-access
class ServeRepoTest(unittest.TestCase):
    """Unittests for serve_repo.py."""

    @mock.patch('serve_repo.run_ffx_command')
    def test_start_server(self, mock_ffx) -> None:
        """Test |_start_serving| function for start."""

        serve_repo._start_serving(_REPO_DIR, _REPO_NAME, _TARGET)
        self.assertEqual(mock_ffx.call_count, 4)
        second_call = mock_ffx.call_args_list[1]
        self.assertEqual(mock.call(cmd=['repository', 'server', 'start']),
                         second_call)
        third_call = mock_ffx.call_args_list[2]
        self.assertEqual(
            mock.call(
                cmd=['repository', 'add-from-pm', _REPO_DIR, '-r', _REPO_NAME
                     ]), third_call)
        fourth_call = mock_ffx.call_args_list[3]
        self.assertEqual(
            mock.call(cmd=[
                'target', 'repository', 'register', '-r', _REPO_NAME,
                '--alias', REPO_ALIAS
            ],
                      target_id=_TARGET), fourth_call)

    @mock.patch('serve_repo.run_ffx_command')
    def test_stop_server(self, mock_ffx) -> None:
        """Test |_stop_serving| function for stop."""

        serve_repo._stop_serving(_REPO_NAME, _TARGET)
        self.assertEqual(mock_ffx.call_count, 3)
        first_call = mock_ffx.call_args_list[0]
        self.assertEqual(
            mock.call(
                cmd=['target', 'repository', 'deregister', '-r', _REPO_NAME],
                target_id=_TARGET,
                check=False), first_call)
        second_call = mock_ffx.call_args_list[1]
        self.assertEqual(
            mock.call(cmd=['repository', 'remove', _REPO_NAME], check=False),
            second_call)
        third_call = mock_ffx.call_args_list[2]
        self.assertEqual(
            mock.call(cmd=['repository', 'server', 'stop'], check=False),
            third_call)

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

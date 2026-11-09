#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing test_env_setup.py."""

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

import test_env_setup


class TestEnvSetupTests(unittest.TestCase):
    """Unittests for test_env_setup.py."""

    def test_expose_env_writes_and_removes_pid_file(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            pid_file = os.path.join(logs_dir, 'test_env_setup.4321.pid')
            captured_env = {}

            def check_pid_file() -> None:
                self.assertTrue(os.path.isfile(pid_file))
                with open(pid_file, 'r') as f:
                    captured_env.update(json.load(f))

            with mock.patch(
                'test_env_setup.wait_for_sigterm', side_effect=check_pid_file
            ), mock.patch.dict(os.environ, {'TEST_EXPOSED_VAR': 'exposed_val'}):
                ret = test_env_setup.expose_env(logs_dir, 4321)

            self.assertEqual(ret, 0)
            self.assertEqual(
                captured_env.get('TEST_EXPOSED_VAR'), 'exposed_val'
            )
            self.assertFalse(os.path.exists(pid_file))

    def test_expose_env_defaults_to_current_pid(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            pid_file = os.path.join(logs_dir, 'test_env_setup.9999.pid')

            with mock.patch('os.getpid', return_value=9999), mock.patch(
                'test_env_setup.wait_for_sigterm',
                side_effect=lambda: self.assertTrue(os.path.isfile(pid_file)),
            ):
                ret = test_env_setup.expose_env(logs_dir)

            self.assertEqual(ret, 0)
            self.assertFalse(os.path.exists(pid_file))

    def test_wait_for_env_setup_loads_environ(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            mock_proc = mock.MagicMock()
            mock_proc.pid = 5678
            pid_file = os.path.join(logs_dir, 'test_env_setup.5678.pid')
            with open(pid_file, 'w') as f:
                json.dump({'ORCHESTRATE_TEST_VAR': 'from_pid_file'}, f)

            with mock.patch.dict(os.environ, {}, clear=False):
                os.environ.pop('ORCHESTRATE_TEST_VAR', None)
                self.assertTrue(
                    test_env_setup.wait_for_env_setup(mock_proc, logs_dir)
                )
                self.assertEqual(
                    os.environ.get('ORCHESTRATE_TEST_VAR'), 'from_pid_file'
                )

    def test_wait_for_env_setup_returns_false_on_proc_exit(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            mock_proc = mock.MagicMock()
            mock_proc.pid = 5678
            mock_proc.returncode = 1
            self.assertFalse(
                test_env_setup.wait_for_env_setup(mock_proc, logs_dir)
            )

    def test_load_exposed_env_loads_pid_file(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            with open(
                os.path.join(logs_dir, 'test_env_setup.7777.pid'), 'w'
            ) as f:
                json.dump({'DISCOVERED_VAR': 'discovered_val'}, f)

            with mock.patch.dict(os.environ, {}, clear=False):
                os.environ.pop('DISCOVERED_VAR', None)
                self.assertTrue(test_env_setup.load_exposed_env(logs_dir, 7777))
                self.assertEqual(
                    os.environ.get('DISCOVERED_VAR'), 'discovered_val'
                )

    def test_load_exposed_env_returns_false_when_missing(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            self.assertFalse(test_env_setup.load_exposed_env(logs_dir, 1234))

    def test_setup_env_passes_test_env_expose_and_mypid(self) -> None:
        argv = ['test_env.py', '--orchestrate']
        with mock.patch.object(sys, 'argv', argv), mock.patch(
            'test_env_setup.catch_sigterm'
        ), mock.patch.object(
            test_env_setup.run_test, '_get_test_runner'
        ), mock.patch('run_test.main', return_value=0) as mock_main:
            self.assertEqual(test_env_setup.setup_env(2468), 0)
            mock_main.assert_called_once()
            self.assertEqual(
                argv,
                [
                    'test_env.py',
                    '--orchestrate',
                    'test_env_expose.py',
                    '--mypid=2468',
                ],
            )


if __name__ == '__main__':
    unittest.main()

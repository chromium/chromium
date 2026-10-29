#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing orchestrate_runner.py."""

import json
import unittest
from unittest import mock

import orchestrate_runner


class OrchestrateRunnerTest(unittest.TestCase):
    """Unittests for orchestrate_runner.py."""

    def test_support_orchestrate(self) -> None:
        self.assertTrue(
            orchestrate_runner.support_orchestrate('base_unittests')
        )
        self.assertTrue(
            orchestrate_runner.support_orchestrate('test_env_expose.py')
        )
        self.assertTrue(
            orchestrate_runner.support_orchestrate('web_engine_unittests')
        )
        self.assertFalse(orchestrate_runner.support_orchestrate('unknown_test'))

    def test_run_tests_with_orchestrate_missing_out_dir(self) -> None:
        with self.assertRaises(ValueError):
            orchestrate_runner.run_tests_with_orchestrate(
                out_dir='', packages=[], target_cmd=['target']
            )

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch('common.get_host_arch', return_value='x64')
    def test_run_tests_with_orchestrate_x64(
        self, _, mock_popen: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.return_value = 0

        packages = [
            'gen/base/base_unittests/base_unittests.far',
        ]
        target_cmd = ['run_executable_test.py', '--test-name', 'base_unittests']
        logs_dir = '/tmp/test_logs'

        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=packages,
            target_cmd=target_cmd,
            logs_dir=logs_dir,
        )
        self.assertEqual(ret, 0)
        mock_popen.assert_called_once()
        cmd = mock_popen.call_args[0][0]
        self.assertIn('orchestrate', cmd[0])
        self.assertEqual(cmd[1], 'run')
        self.assertEqual(cmd[2], '-input')
        self.assertTrue(cmd[3].endswith('orchestrate_x64.json'))
        self.assertEqual(cmd[4], '-overrides')
        overrides = json.loads(cmd[5])
        self.assertEqual(overrides['emulator']['package_archives'], packages)
        self.assertEqual(
            overrides['emulator']['build_ids'],
            ['gen/base/base_unittests/ids.txt'],
        )
        self.assertEqual(cmd[6], '--')
        self.assertEqual(cmd[7:], target_cmd)

        env = mock_popen.call_args[1]['env']
        self.assertEqual(env['TEST_UNDECLARED_OUTPUTS_DIR'], logs_dir)
        self.assertEqual(mock_popen.call_args[1]['cwd'], 'out/Release')

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch('common.get_host_arch', return_value='arm64')
    def test_run_tests_with_orchestrate_arm64(
        self, _, mock_popen: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.return_value = 0

        packages = ['gen/base/base_unittests/base_unittests.far']
        target_cmd = ['run_executable_test.py']

        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=packages,
            target_cmd=target_cmd,
        )
        self.assertEqual(ret, 0)
        mock_popen.assert_called_once()
        cmd = mock_popen.call_args[0][0]
        self.assertTrue(cmd[3].endswith('orchestrate_arm64.json'))

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch('common.get_host_arch', return_value='x64')
    def test_run_tests_with_orchestrate_keyboard_interrupt(
        self, _, mock_popen: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.side_effect = KeyboardInterrupt()
        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=[],
            target_cmd=['target'],
        )
        self.assertEqual(ret, 130)
        mock_proc.terminate.assert_called_once()

    @mock.patch('common.catch_sigterm')
    @mock.patch('subprocess.Popen')
    @mock.patch('common.get_host_arch', return_value='x64')
    def test_run_tests_with_orchestrate_system_exit(
        self, _, mock_popen: mock.MagicMock, mock_catch_sigterm: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.side_effect = [SystemExit(0), 0]
        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=[],
            target_cmd=['target'],
        )
        self.assertEqual(ret, 0)
        mock_catch_sigterm.assert_called_once()
        mock_proc.terminate.assert_called_once()
        self.assertEqual(mock_proc.wait.call_count, 2)


if __name__ == '__main__':
    unittest.main()

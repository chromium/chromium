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
            orchestrate_runner.support_orchestrate('web_engine_unittests')
        )
        self.assertFalse(orchestrate_runner.support_orchestrate('unknown_test'))

    def test_run_tests_with_orchestrate_missing_out_dir(self) -> None:
        with self.assertRaises(ValueError):
            orchestrate_runner.run_tests_with_orchestrate(
                out_dir='', packages=[], target_cmd=['target']
            )

    @mock.patch('subprocess.run')
    @mock.patch('common.get_host_arch', return_value='x64')
    def test_run_tests_with_orchestrate_x64(
        self, _, mock_run: mock.MagicMock
    ) -> None:
        mock_proc = mock.MagicMock()
        mock_proc.returncode = 0
        mock_run.return_value = mock_proc

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
        mock_run.assert_called_once()
        cmd = mock_run.call_args[0][0]
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

        env = mock_run.call_args[1]['env']
        self.assertEqual(env['TEST_UNDECLARED_OUTPUTS_DIR'], logs_dir)
        self.assertEqual(mock_run.call_args[1]['cwd'], 'out/Release')

    @mock.patch('subprocess.run')
    @mock.patch('common.get_host_arch', return_value='arm64')
    def test_run_tests_with_orchestrate_arm64(
        self, _, mock_run: mock.MagicMock
    ) -> None:
        mock_proc = mock.MagicMock()
        mock_proc.returncode = 0
        mock_run.return_value = mock_proc

        packages = ['gen/base/base_unittests/base_unittests.far']
        target_cmd = ['run_executable_test.py']

        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=packages,
            target_cmd=target_cmd,
        )
        self.assertEqual(ret, 0)
        mock_run.assert_called_once()
        cmd = mock_run.call_args[0][0]
        self.assertTrue(cmd[3].endswith('orchestrate_arm64.json'))

    @mock.patch('subprocess.run')
    @mock.patch('common.get_host_arch', return_value='x64')
    def test_run_tests_with_orchestrate_keyboard_interrupt(
        self, _, mock_run: mock.MagicMock
    ) -> None:
        mock_run.side_effect = KeyboardInterrupt()
        ret = orchestrate_runner.run_tests_with_orchestrate(
            out_dir='out/Release',
            packages=[],
            target_cmd=['target'],
        )
        self.assertEqual(ret, 130)


if __name__ == '__main__':
    unittest.main()

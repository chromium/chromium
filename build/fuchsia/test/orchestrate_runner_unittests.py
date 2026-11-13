#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing orchestrate_runner.py."""

import json
import os
import tempfile
import unittest
from unittest import mock

import orchestrate_runner


class OrchestrateRunnerTest(unittest.TestCase):
    """Unittests for orchestrate_runner.py."""

    def test_orchestrate_configs_error_on_test_failure(self) -> None:
        for arch in ('x64', 'arm64'):
            config_path = os.path.join(
                orchestrate_runner.SCRIPT_DIR, f'orchestrate_{arch}.json'
            )
            with open(config_path, encoding='utf-8') as f:
                config = json.load(f)
            self.assertIn(
                'orchestrate-error-on-test-failure',
                config.get('experiments', []),
            )

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
        self.assertFalse(
            orchestrate_runner.support_orchestrate('bundled_test_runner.py')
        )
        self.assertFalse(orchestrate_runner.support_orchestrate('unknown_test'))

    def test_run_tests_with_orchestrate_missing_out_dir(self) -> None:
        with self.assertRaises(ValueError):
            orchestrate_runner.run_tests_with_orchestrate(
                out_dir='', packages=[], target_cmd=['target']
            )

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch(
        'common.ORCHESTRATE_CONFIG',
        new=os.path.join(orchestrate_runner.SCRIPT_DIR, 'orchestrate_x64.json'),
    )
    def test_run_tests_with_orchestrate_x64(
        self, mock_popen: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.return_value = 0

        with tempfile.TemporaryDirectory() as out_dir:
            ids_dir = os.path.join(out_dir, 'gen', 'base', 'base_unittests')
            os.makedirs(ids_dir)
            with open(os.path.join(ids_dir, 'ids.txt'), 'w') as f:
                f.write('')

            packages = [
                'gen/base/base_unittests/base_unittests.far',
                'gen/base/base_unittests/extra.far',
                'gen/base/base_unittests/base_unittests.far',
                'missing_ids/pkg.far',
            ]
            target_cmd = [
                'run_executable_test.py',
                '--test-name',
                'base_unittests',
            ]
            logs_dir = '/tmp/test_logs'

            ret = orchestrate_runner.run_tests_with_orchestrate(
                out_dir=out_dir,
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
            self.assertEqual(
                overrides['emulator']['package_archives'],
                [
                    'gen/base/base_unittests/base_unittests.far',
                    'gen/base/base_unittests/extra.far',
                    'missing_ids/pkg.far',
                ],
            )
            self.assertEqual(
                overrides['emulator']['build_ids'],
                ['gen/base/base_unittests/ids.txt'],
            )
            self.assertEqual(cmd[6], '--')
            self.assertEqual(cmd[7:], target_cmd)

            env = mock_popen.call_args[1]['env']
            self.assertEqual(env['TEST_UNDECLARED_OUTPUTS_DIR'], logs_dir)
            self.assertEqual(mock_popen.call_args[1]['cwd'], out_dir)

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch(
        'common.ORCHESTRATE_CONFIG',
        new=os.path.join(
            orchestrate_runner.SCRIPT_DIR, 'orchestrate_arm64.json'
        ),
    )
    def test_run_tests_with_orchestrate_arm64_without_ids_txt(
        self, mock_popen: mock.MagicMock
    ) -> None:
        mock_proc = mock_popen.return_value.__enter__.return_value
        mock_proc.wait.return_value = 0

        with tempfile.TemporaryDirectory() as out_dir:
            packages = ['assets_unittests.far']
            target_cmd = ['run_executable_test.py']

            ret = orchestrate_runner.run_tests_with_orchestrate(
                out_dir=out_dir,
                packages=packages,
                target_cmd=target_cmd,
            )
            self.assertEqual(ret, 0)
            mock_popen.assert_called_once()
            cmd = mock_popen.call_args[0][0]
            self.assertTrue(cmd[3].endswith('orchestrate_arm64.json'))
            self.assertEqual(cmd[4], '-overrides')
            self.assertEqual(
                json.loads(cmd[5]),
                {'emulator': {'package_archives': ['assets_unittests.far']}},
            )

    @mock.patch('common.catch_sigterm', new=lambda: None)
    @mock.patch('subprocess.Popen')
    @mock.patch(
        'common.ORCHESTRATE_CONFIG',
        new=os.path.join(orchestrate_runner.SCRIPT_DIR, 'orchestrate_x64.json'),
    )
    def test_run_tests_with_orchestrate_keyboard_interrupt(
        self, mock_popen: mock.MagicMock
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
    @mock.patch(
        'common.ORCHESTRATE_CONFIG',
        new=os.path.join(orchestrate_runner.SCRIPT_DIR, 'orchestrate_x64.json'),
    )
    def test_run_tests_with_orchestrate_system_exit(
        self, mock_popen: mock.MagicMock, mock_catch_sigterm: mock.MagicMock
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
        self.assertNotIn('-overrides', mock_popen.call_args[0][0])


if __name__ == '__main__':
    unittest.main()

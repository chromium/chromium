#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing run_test.py."""

import os
import sys
import unittest
from unittest import mock

import bundled_test_runner
import run_test
import test_env_setup


class RunTestUnittests(unittest.TestCase):
    """Unittests for run_test.py."""

    def setUp(self) -> None:
        self._orig_get_test_runner = run_test._get_test_runner

    def tearDown(self) -> None:
        run_test._get_test_runner = self._orig_get_test_runner

    def test_orchestrate_supports_arbitrary_executable_tests(self) -> None:
        for test_name in (
            'angle_unittests',
            'flatbuffers_unittests',
            'custom_new_unittests',
        ):
            with self.subTest(test_name=test_name), mock.patch.dict(
                os.environ, {}, clear=False
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_test.py',
                    test_name,
                    '--orchestrate',
                    '--out-dir=/abs/out',
                ],
            ), mock.patch(
                'run_test.get_host_arch', return_value='x64'
            ), mock.patch(
                'run_test.read_package_paths',
                return_value=[f'{test_name}.far'],
            ), mock.patch(
                'run_test.run_tests_with_orchestrate', return_value=0
            ) as mock_orch:
                os.environ.pop('FUCHSIA_NODENAME', None)
                self.assertEqual(run_test.main(), 0)
                mock_orch.assert_called_once()

    def test_orchestrate_supports_test_env_setup(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'test_env_setup.py',
                '--orchestrate',
                '--out-dir=/abs/out',
                '--logs-dir=/tmp/logs',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.run_tests_with_orchestrate', return_value=0
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            self.assertEqual(test_env_setup.setup_env(mypid=1234), 0)
            mock_orch.assert_called_once()

    def test_denylisted_test_types_fall_back_to_homemade(self) -> None:
        for test_type in ('blink', 'gpu', 'perf', 'perf_gtest', 'webpage'):
            with self.subTest(test_type=test_type), mock.patch.dict(
                os.environ, {}, clear=False
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_test.py',
                    test_type,
                    '--orchestrate',
                    '--out-dir=/abs/out',
                ],
            ), mock.patch(
                'run_test.get_host_arch', return_value='x64'
            ), mock.patch(
                'run_test.running_unattended', return_value=False
            ), mock.patch('run_test.LogManager'), mock.patch(
                'run_test.create_emulator_from_args'
            ), mock.patch('run_test.test_connection'), mock.patch(
                'run_test.start_system_log'
            ), mock.patch(
                'run_test._get_test_runner'
            ) as mock_runner, mock.patch(
                'run_test.run_tests_with_orchestrate'
            ) as mock_orch:
                os.environ.pop('FUCHSIA_NODENAME', None)
                mock_runner.return_value.package_deps = {}
                mock_runner.return_value.run_test.return_value.returncode = 0
                self.assertEqual(run_test.main(), 0)
                mock_orch.assert_not_called()
                mock_runner.assert_called_once()

    def test_bundled_test_runner_falls_back_to_homemade(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'bundled_test_runner.py',
                '--orchestrate',
                '--out-dir=/abs/out',
                '--packages=pkg.far',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.running_unattended', return_value=False
        ), mock.patch('run_test.LogManager'), mock.patch(
            'run_test.create_emulator_from_args'
        ), mock.patch('run_test.test_connection'), mock.patch(
            'run_test.start_system_log'
        ), mock.patch(
            'run_test.ensure_repository', return_value=True
        ), mock.patch('run_test.serve_repository'), mock.patch(
            'run_test.publish_packages'
        ), mock.patch('run_test.resolve_packages'), mock.patch(
            'bundled_test_runner.ExecutableTestRunner'
        ) as mock_exec_runner, mock.patch(
            'run_test.run_tests_with_orchestrate'
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            mock_exec_runner.return_value.run_test.return_value.returncode = 0
            self.assertEqual(
                bundled_test_runner.run_tests(
                    [
                        bundled_test_runner.TestCase(
                            package='fuchsia-pkg://fuchsia.com/pkg#meta/pkg.cm'
                        )
                    ]
                ),
                0,
            )
            mock_orch.assert_not_called()
            mock_exec_runner.assert_called_once()

    def test_incompatible_flags_fall_back_to_homemade(self) -> None:
        for extra_flag in (
            '--device',
            '--target-id=fuchsia-node',
            '--everlasting',
            '--product=smart_display_eng_arrested.x64',
            '--custom-image=/abs/pb',
            '--wait-for-log-pattern=CrashIntentionally',
            '--no-publish-package',
        ):
            with self.subTest(extra_flag=extra_flag), mock.patch.dict(
                os.environ, {}, clear=False
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_test.py',
                    'content_shell',
                    '--orchestrate',
                    '--out-dir=/abs/out',
                    extra_flag,
                ],
            ), mock.patch(
                'run_test.get_host_arch', return_value='x64'
            ), mock.patch(
                'run_test.running_unattended', return_value=False
            ), mock.patch('run_test.LogManager'), mock.patch(
                'run_test.update'
            ), mock.patch('run_test.test_device_connection'), mock.patch(
                'run_test.create_emulator_from_args'
            ), mock.patch('run_test.test_connection'), mock.patch(
                'run_test.start_system_log'
            ), mock.patch(
                'run_test._get_test_runner'
            ) as mock_runner, mock.patch(
                'run_test.run_tests_with_orchestrate'
            ) as mock_orch:
                os.environ.pop('FUCHSIA_NODENAME', None)
                mock_runner.return_value.package_deps = {}
                mock_runner.return_value.run_test.return_value.returncode = 0
                self.assertEqual(run_test.main(), 0)
                mock_orch.assert_not_called()
                mock_runner.assert_called_once()

    def test_no_publish_package_skips_repo_publish_and_resolve(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--device',
                '--no-publish-package',
                '--out-dir=/abs/out',
            ],
        ), mock.patch(
            'run_test.running_unattended', return_value=False
        ), mock.patch('run_test.LogManager'), mock.patch(
            'run_test.update'
        ), mock.patch('run_test.test_device_connection'), mock.patch(
            'run_test.start_system_log'
        ) as mock_start_log, mock.patch(
            'run_test.ensure_repository'
        ) as mock_ensure_repo, mock.patch(
            'run_test.serve_repository'
        ) as mock_serve_repo, mock.patch(
            'run_test.publish_packages'
        ) as mock_publish, mock.patch(
            'run_test.resolve_packages'
        ) as mock_resolve, mock.patch(
            'run_test._get_test_runner'
        ) as mock_runner:
            os.environ.pop('FUCHSIA_NODENAME', None)
            mock_runner.return_value.package_deps = {
                'base_unittests': '/abs/out/base_unittests.far'
            }
            mock_runner.return_value.run_test.return_value.returncode = 0
            self.assertEqual(run_test.main(), 0)
            mock_ensure_repo.assert_not_called()
            mock_serve_repo.assert_not_called()
            mock_publish.assert_not_called()
            mock_resolve.assert_not_called()
            mock_start_log.assert_called_once()
            mock_runner.return_value.run_test.assert_called_once()


if __name__ == '__main__':
    unittest.main()

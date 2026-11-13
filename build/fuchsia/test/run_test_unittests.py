#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing run_test.py."""

import os
import sys
import unittest
from unittest import mock

import run_executable_test
import run_test


class RunTestUnittests(unittest.TestCase):
    """Unittests for run_test.py."""

    def test_main_uses_orchestrate_by_default(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--out-dir=/abs/out',
                '--gtest_filter=Foo.Bar',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.read_package_paths', return_value=['base_unittests.far']
        ), mock.patch(
            'run_test.run_tests_with_orchestrate', return_value=0
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            self.assertEqual(run_test.main(), 0)
            mock_orch.assert_called_once()
            target_cmd = mock_orch.call_args[0][2]
            self.assertEqual(
                target_cmd[-3:],
                ['--out-dir', '/abs/out', '--gtest_filter=Foo.Bar'],
            )

    def test_main_ignores_orchestrate_flag(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--orchestrate',
                '--out-dir=/abs/out',
                '--gtest_filter=Foo.Bar',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.read_package_paths', return_value=['base_unittests.far']
        ), mock.patch(
            'run_test.run_tests_with_orchestrate', return_value=0
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            self.assertEqual(run_test.main(), 0)
            mock_orch.assert_called_once()
            self.assertNotIn('--orchestrate', mock_orch.call_args[0][2])

    def test_main_no_orchestrate_falls_back_to_homemade(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--orchestrate',
                '--no-orchestrate',
                '--out-dir=/abs/out',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.running_unattended', return_value=False
        ), mock.patch('run_test.LogManager'), mock.patch(
            'run_test.create_emulator_from_args'
        ), mock.patch('run_test.test_connection'), mock.patch(
            'run_test.start_system_log'
        ), mock.patch(
            'run_test._get_test_runner'
        ) as mock_get_runner, mock.patch(
            'run_test.run_tests_with_orchestrate'
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            mock_get_runner.return_value.package_deps = {}
            mock_get_runner.return_value.run_test.return_value.returncode = 0
            self.assertEqual(run_test.main(), 0)
            mock_orch.assert_not_called()
            mock_get_runner.assert_called_once()

    def test_main_everlasting_falls_back_to_homemade(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--everlasting',
                '--out-dir=/abs/out',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.running_unattended', return_value=False
        ), mock.patch('run_test.LogManager'), mock.patch(
            'run_test.create_emulator_from_args'
        ), mock.patch('run_test.test_connection'), mock.patch(
            'run_test.start_system_log'
        ), mock.patch(
            'run_test._get_test_runner'
        ) as mock_get_runner, mock.patch(
            'run_test.run_tests_with_orchestrate'
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            mock_get_runner.return_value.package_deps = {}
            mock_get_runner.return_value.run_test.return_value.returncode = 0
            self.assertEqual(run_test.main(), 0)
            mock_orch.assert_not_called()
            mock_get_runner.assert_called_once()

    def test_main_forwards_executable_test_args(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=False), mock.patch.object(
            sys,
            'argv',
            [
                'run_test.py',
                'base_unittests',
                '--out-dir=/abs/out',
                '--code-coverage-dir=/cov',
                '--test-realm=/core/testing:system-tests',
                '--package-deps=a.far',
                '--package-deps=b.far',
                '--gtest_filter=Foo.Bar',
            ],
        ), mock.patch('run_test.get_host_arch', return_value='x64'), mock.patch(
            'run_test.read_package_paths'
        ) as mock_read_pkgs, mock.patch(
            'run_test.run_tests_with_orchestrate', return_value=0
        ) as mock_orch:
            os.environ.pop('FUCHSIA_NODENAME', None)
            self.assertEqual(run_test.main(), 0)
            mock_read_pkgs.assert_not_called()
            mock_orch.assert_called_once()
            _, packages, target_cmd, _ = mock_orch.call_args[0]
            self.assertEqual(packages, ['a.far', 'b.far'])

        with mock.patch.object(sys, 'argv', target_cmd), mock.patch(
            'run_executable_test.create_executable_test_runner'
        ) as mock_create_runner:
            mock_create_runner.return_value.run_test.return_value.returncode = 0
            self.assertEqual(run_executable_test.main(), 0)
            parsed_args, remaining = mock_create_runner.call_args[0]
            self.assertEqual(parsed_args.test_type, 'base_unittests')
            self.assertEqual(parsed_args.out_dir, '/abs/out')
            self.assertEqual(parsed_args.code_coverage_dir, '/cov')
            self.assertEqual(
                parsed_args.test_realm, '/core/testing:system-tests'
            )
            self.assertEqual(parsed_args.package_deps, ['a.far', 'b.far'])
            self.assertEqual(remaining, ['--gtest_filter=Foo.Bar'])


if __name__ == '__main__':
    unittest.main()

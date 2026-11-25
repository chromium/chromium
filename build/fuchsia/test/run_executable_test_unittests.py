#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing run_executable_test.py."""

import json
import os
import sys
import tempfile
import unittest
from typing import List, Optional
from unittest import mock

import run_executable_test


class RunExecutableTestUnittests(unittest.TestCase):
    """Unittests for run_executable_test.py."""

    @staticmethod
    def _create_runner(
        test_name: str = 'base_unittests',
        test_args: Optional[List[str]] = None,
    ) -> run_executable_test.ExecutableTestRunner:
        return run_executable_test.ExecutableTestRunner(
            out_dir='/abs/out',
            test_args=test_args or [],
            test_name=test_name,
            target_id=None,
            code_coverage_dir='',
            logs_dir=None,
            package_deps={test_name: f'{test_name}.far'},
            test_realm=None,
        )

    def test_main_loads_exposed_env_from_ffx_isolate_dir(self) -> None:
        with tempfile.TemporaryDirectory() as isolate_dir:
            with open(
                os.path.join(isolate_dir, 'test_env_setup.1234.pid'), 'w'
            ) as f:
                json.dump(
                    {
                        'FFX_ISOLATE_DIR': '/tmp/orch_isolate',
                        'FUCHSIA_NODENAME': 'fuchsia-emulator-1234',
                    },
                    f,
                )
            with mock.patch.dict(
                os.environ,
                {
                    'FFX_ISOLATE_DIR': isolate_dir,
                    'FUCHSIA_NODENAME': 'stale-outer-target',
                },
                clear=False,
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_executable_test.py',
                    '--mypid=1234',
                    '--out-dir=/abs/out',
                    '--test-name=base_unittests',
                ],
            ), mock.patch(
                'run_executable_test.create_executable_test_runner'
            ) as mock_create:
                mock_create.return_value.run_test.return_value.returncode = 0
                self.assertEqual(run_executable_test.main(), 0)
                self.assertEqual(
                    os.environ['FFX_ISOLATE_DIR'], '/tmp/orch_isolate'
                )
                self.assertEqual(
                    mock_create.call_args[0][0].target_id,
                    'fuchsia-emulator-1234',
                )
                self.assertEqual(mock_create.call_args[0][1], [])

    def test_main_loads_exposed_env_from_logs_dir(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            with open(
                os.path.join(logs_dir, 'test_env_setup.5678.pid'), 'w'
            ) as f:
                json.dump({'FUCHSIA_NODENAME': 'fuchsia-emulator-5678'}, f)
            with mock.patch.dict(
                os.environ, {}, clear=False
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_executable_test.py',
                    '--mypid=5678',
                    f'--logs-dir={logs_dir}',
                    '--out-dir=/abs/out',
                    '--test-name=base_unittests',
                ],
            ), mock.patch(
                'run_executable_test.create_executable_test_runner'
            ) as mock_create:
                os.environ.pop('FFX_ISOLATE_DIR', None)
                os.environ.pop('FUCHSIA_NODENAME', None)
                mock_create.return_value.run_test.return_value.returncode = 0
                self.assertEqual(run_executable_test.main(), 0)
                self.assertEqual(
                    mock_create.call_args[0][0].target_id,
                    'fuchsia-emulator-5678',
                )
                self.assertEqual(mock_create.call_args[0][1], [])

    def test_main_preserves_target_id_when_nodename_absent(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir:
            with open(
                os.path.join(logs_dir, 'test_env_setup.5678.pid'), 'w'
            ) as f:
                json.dump({'FFX_ISOLATE_DIR': '/tmp/orch_isolate'}, f)
            with mock.patch.dict(
                os.environ, {}, clear=False
            ), mock.patch.object(
                sys,
                'argv',
                [
                    'run_executable_test.py',
                    '--mypid=5678',
                    '--target-id=192.168.42.10',
                    f'--logs-dir={logs_dir}',
                    '--out-dir=/abs/out',
                    '--test-name=base_unittests',
                ],
            ), mock.patch(
                'run_executable_test.create_executable_test_runner'
            ) as mock_create:
                os.environ.pop('FUCHSIA_NODENAME', None)
                mock_create.return_value.run_test.return_value.returncode = 0
                self.assertEqual(run_executable_test.main(), 0)
                self.assertEqual(
                    mock_create.call_args[0][0].target_id,
                    '192.168.42.10',
                )

    def test_main_raises_when_mypid_file_missing(self) -> None:
        with tempfile.TemporaryDirectory() as logs_dir, mock.patch.object(
            sys,
            'argv',
            [
                'run_executable_test.py',
                '--mypid=9999',
                f'--logs-dir={logs_dir}',
                '--out-dir=/abs/out',
                '--test-name=base_unittests',
            ],
        ):
            with self.assertRaises(FileNotFoundError) as ctx:
                run_executable_test.main()
            self.assertIn('9999', str(ctx.exception))
            self.assertIn(logs_dir, str(ctx.exception))

    def test_get_args_x64_enables_native_vulkan_and_gpu_in_tests(self) -> None:
        with mock.patch('platform.machine', return_value='x86_64'):
            args = self._create_runner()._get_args()
        self.assertIn('--use-vulkan=native', args)
        self.assertIn('--use-gpu-in-tests', args)
        self.assertFalse(
            any(
                arg.startswith(('--enable-features=', '--disable-features='))
                for arg in args
            )
        )
        self.assertNotIn('--use-vulkan=swiftshader', args)
        self.assertNotIn('--ozone-platform=headless', args)

    def test_get_args_non_x64_uses_swiftshader_and_headless(self) -> None:
        with mock.patch('platform.machine', return_value='aarch64'):
            args = self._create_runner()._get_args()
        self.assertIn('--use-vulkan=swiftshader', args)
        self.assertIn('--ozone-platform=headless', args)
        self.assertIn('--ozone-override-screen-size=800,600', args)
        self.assertNotIn('--use-vulkan=native', args)
        self.assertNotIn('--use-gpu-in-tests', args)
        self.assertFalse(
            any(arg.startswith('--enable-features=') for arg in args)
        )

    def test_get_args_omits_vulkan_for_flatbuffers_unittests(self) -> None:
        for arch in ('x86_64', 'aarch64'):
            with mock.patch('platform.machine', return_value=arch):
                args = self._create_runner(
                    test_name='flatbuffers_unittests'
                )._get_args()
                self.assertFalse(
                    any(arg.startswith('--use-vulkan') for arg in args)
                )
                self.assertNotIn('--use-gpu-in-tests', args)
                self.assertFalse(
                    any(arg.startswith('--enable-features=') for arg in args)
                )
                self.assertNotIn('--ozone-platform=headless', args)
                self.assertNotIn('--ozone-override-screen-size=800,600', args)

    def test_get_args_x64_headless_sets_default_screen_size(self) -> None:
        for test_args in (
            ['--ozone-platform=headless'],
            ['--test-arg=--ozone-platform=headless'],
        ):
            with mock.patch('platform.machine', return_value='x86_64'):
                args = self._create_runner(test_args=test_args)._get_args()
                self.assertIn('--ozone-platform=headless', args)
                self.assertIn('--ozone-override-screen-size=800,600', args)
                self.assertFalse(
                    any(arg.startswith('--use-vulkan') for arg in args)
                )
                self.assertNotIn('--use-gpu-in-tests', args)
                self.assertFalse(
                    any(arg.startswith('--enable-features=') for arg in args)
                )

    def test_get_args_preserves_explicit_screen_override_for_headless(
        self,
    ) -> None:
        with mock.patch('platform.machine', return_value='x86_64'):
            x64_args = self._create_runner(
                test_args=[
                    '--test-arg=--ozone-platform=headless',
                    '--test-arg=--ozone-override-screen-size=1024,768',
                ]
            )._get_args()
        self.assertIn('--ozone-platform=headless', x64_args)
        self.assertIn('--ozone-override-screen-size=1024,768', x64_args)
        self.assertNotIn('--ozone-override-screen-size=800,600', x64_args)
        self.assertFalse(
            any(arg.startswith('--use-vulkan') for arg in x64_args)
        )
        self.assertNotIn('--use-gpu-in-tests', x64_args)
        self.assertFalse(
            any(arg.startswith('--enable-features=') for arg in x64_args)
        )

        with mock.patch('platform.machine', return_value='aarch64'):
            arm64_args = self._create_runner(
                test_args=['--screen-info=800x600']
            )._get_args()
        self.assertIn('--ozone-platform=headless', arm64_args)
        self.assertIn('--screen-info=800x600', arm64_args)
        self.assertNotIn('--ozone-override-screen-size=800,600', arm64_args)

    def test_get_args_x64_omits_use_gpu_in_tests_for_excluded_suites(
        self,
    ) -> None:
        for test_name in (
            'cc_unittests',
            'content_unittests',
            'viz_unittests',
        ):
            with self.subTest(test_name=test_name):
                with mock.patch('platform.machine', return_value='x86_64'):
                    args = self._create_runner(test_name=test_name)._get_args()
                self.assertIn('--use-vulkan=native', args)
                self.assertNotIn('--use-gpu-in-tests', args)
                self.assertFalse(
                    any(arg.startswith('--enable-features=') for arg in args)
                )
                self.assertNotIn('--ozone-platform=headless', args)

    def test_get_args_x64_disables_vulkan_feature_for_media_unittests(
        self,
    ) -> None:
        with mock.patch('platform.machine', return_value='x86_64'):
            args = self._create_runner(test_name='media_unittests')._get_args()
        self.assertIn('--use-vulkan=native', args)
        self.assertIn('--use-gpu-in-tests', args)
        self.assertIn('--disable-features=Vulkan', args)
        self.assertFalse(
            any(arg.startswith('--enable-features=') for arg in args)
        )
        self.assertNotIn('--ozone-platform=headless', args)


if __name__ == '__main__':
    unittest.main()

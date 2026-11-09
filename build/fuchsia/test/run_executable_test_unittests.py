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
from unittest import mock

import run_executable_test


class RunExecutableTestUnittests(unittest.TestCase):
    """Unittests for run_executable_test.py."""

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


if __name__ == '__main__':
    unittest.main()

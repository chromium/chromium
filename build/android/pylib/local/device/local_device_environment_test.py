#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import os
import sys
import unittest
from unittest import mock

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
)

# pylib must be imported first to add devil to sys.path.
from pylib.local.device import local_device_environment
from devil.android import device_errors

_SERIAL = 'test-device-serial'


def _create_env():
    args = mock.MagicMock()
    args.denylist_file = None
    args.test_devices = [_SERIAL]
    args.enable_concurrent_adb = False
    args.enable_device_cache = False
    args.logcat_output_dir = None
    args.logcat_output_file = None
    args.trace_output = None
    args.trace_all = None
    args.num_retries = 0
    args.device_retries = 0
    args.recover_devices = False
    args.skip_clear_data = False
    args.force_main_user = False
    args.disable_persistent_shell = True
    args.adb_path = None
    with (
        mock.patch('devil_chromium.Initialize'),
        mock.patch(
            'devil.android.sdk.adb_wrapper.AdbWrapper.GetAdbPath',
            return_value='/dummy/adb',
        ),
    ):
        return local_device_environment.LocalDeviceEnvironment(
            args, mock.MagicMock(), None
        )


def _mock_device():
    device = mock.MagicMock()
    device.adb.GetDeviceSerial.return_value = _SERIAL
    device.is_desktop = False
    device.GetCurrentUser.return_value = 0
    device.build_version_sdk = 33
    return device


class LocalDeviceEnvironmentTest(unittest.TestCase):
    def setUp(self):
        patcher = mock.patch(
            'pylib.constants.GetOutDirectory', return_value='/dummy/out'
        )
        patcher.start()
        self.addCleanup(patcher.stop)

    @mock.patch('devil.android.device_utils.DeviceUtils.HealthyDevices')
    def test_prepare_device_saves_animation_scales(self, mock_healthy_devices):
        device = _mock_device()
        mock_healthy_devices.return_value = [device]

        expected_cmd = (
            'settings get global window_animation_scale && '
            'settings get global transition_animation_scale && '
            'settings get global animator_duration_scale'
        )

        def run_shell(cmd, *args, **kwargs):
            if cmd == ['date', '+%s']:
                return '1234567890'
            if cmd == expected_cmd:
                return ['1.0', '0.5', 'null']
            return ''

        device.RunShellCommand.side_effect = run_shell

        env = _create_env()
        with mock.patch(
            'devil.utils.parallelizer.SyncParallelizer.pMap',
            side_effect=lambda f: [f(d) for d in env._devices],
        ):
            env._InitDevices()

        device.RunShellCommand.assert_any_call(
            expected_cmd, shell=True, check_return=True
        )
        self.assertEqual(
            env._saved_animation_scales.get(_SERIAL),
            {
                'window_animation_scale': '1.0',
                'transition_animation_scale': '0.5',
                'animator_duration_scale': '1.0',
            },
        )

    @mock.patch('devil.android.device_utils.DeviceUtils.HealthyDevices')
    def test_prepare_device_query_failure_handled(self, mock_healthy_devices):
        device = _mock_device()
        mock_healthy_devices.return_value = [device]

        expected_cmd = (
            'settings get global window_animation_scale && '
            'settings get global transition_animation_scale && '
            'settings get global animator_duration_scale'
        )

        def run_shell(cmd, *args, **kwargs):
            if cmd == ['date', '+%s']:
                return '1234567890'
            if cmd == expected_cmd:
                raise device_errors.CommandFailedError('Failed to query')
            return ''

        device.RunShellCommand.side_effect = run_shell

        env = _create_env()
        with mock.patch(
            'devil.utils.parallelizer.SyncParallelizer.pMap',
            side_effect=lambda f: [f(d) for d in env._devices],
        ):
            env._InitDevices()

        self.assertNotIn(_SERIAL, env._saved_animation_scales)

    @mock.patch('devil.android.device_utils.DeviceUtils.HealthyDevices')
    def test_prepare_device_all_zero_logs_warning(self, mock_healthy_devices):
        device = _mock_device()
        mock_healthy_devices.return_value = [device]

        expected_cmd = (
            'settings get global window_animation_scale && '
            'settings get global transition_animation_scale && '
            'settings get global animator_duration_scale'
        )

        def run_shell(cmd, *args, **kwargs):
            if cmd == ['date', '+%s']:
                return '1234567890'
            if cmd == expected_cmd:
                return ['0.0', '0', '0.0']
            return ''

        device.RunShellCommand.side_effect = run_shell

        env = _create_env()
        with (
            mock.patch(
                'devil.utils.parallelizer.SyncParallelizer.pMap',
                side_effect=lambda f: [f(d) for d in env._devices],
            ),
            self.assertLogs(level='WARNING') as cm,
        ):
            env._InitDevices()

        self.assertTrue(
            any('were all 0.0 at startup' in msg for msg in cm.output)
        )
        self.assertEqual(
            env._saved_animation_scales.get(_SERIAL),
            {
                'window_animation_scale': '0.0',
                'transition_animation_scale': '0',
                'animator_duration_scale': '0.0',
            },
        )

    def test_tear_down_device_restores_animation_scales(self):
        device = _mock_device()
        env = _create_env()
        env._devices = [device]
        env._saved_animation_scales = {
            _SERIAL: {
                'window_animation_scale': '1.0',
                'transition_animation_scale': '0.5',
                'animator_duration_scale': '1.0',
            }
        }

        with (
            mock.patch(
                'devil.utils.parallelizer.SyncParallelizer.pMap',
                side_effect=lambda f: [f(d) for d in env._devices],
            ),
            mock.patch('os.path.exists', return_value=False),
        ):
            env.TearDown()

        expected_cmd = (
            'settings put global window_animation_scale 1.0 && '
            'settings put global transition_animation_scale 0.5 && '
            'settings put global animator_duration_scale 1.0'
        )
        device.RunShellCommand.assert_called_once_with(
            expected_cmd, shell=True, check_return=True
        )

    def test_tear_down_device_skips_when_all_zero(self):
        device = _mock_device()
        env = _create_env()
        env._devices = [device]
        env._saved_animation_scales = {
            _SERIAL: {
                'window_animation_scale': '0.0',
                'transition_animation_scale': '0',
                'animator_duration_scale': '0.0',
            }
        }

        with (
            mock.patch(
                'devil.utils.parallelizer.SyncParallelizer.pMap',
                side_effect=lambda f: [f(d) for d in env._devices],
            ),
            mock.patch('os.path.exists', return_value=False),
        ):
            env.TearDown()

        device.RunShellCommand.assert_not_called()

    def test_tear_down_device_restore_failure_handled(self):
        device = _mock_device()
        device.RunShellCommand.side_effect = device_errors.CommandFailedError(
            'Failed to put'
        )
        env = _create_env()
        env._devices = [device]
        env._saved_animation_scales = {
            _SERIAL: {
                'window_animation_scale': '1.0',
                'transition_animation_scale': '0.5',
            }
        }

        with (
            mock.patch(
                'devil.utils.parallelizer.SyncParallelizer.pMap',
                side_effect=lambda f: [f(d) for d in env._devices],
            ),
            mock.patch('os.path.exists', return_value=False),
            self.assertLogs(level='WARNING') as cm,
        ):
            env.TearDown()

        self.assertTrue(
            any(
                'Failed to restore animation scales' in msg for msg in cm.output
            )
        )


if __name__ == '__main__':
    unittest.main()

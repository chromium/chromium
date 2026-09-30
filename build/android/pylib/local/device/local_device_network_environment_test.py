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
from pylib.local.device import local_device_network_environment
from devil.android import device_errors

_DEVICE = '192.168.1.100:5555'
_SUPER_INIT = (
    'pylib.local.device.local_device_environment.'
    'LocalDeviceEnvironment.__init__'
)
_SUPER_SETUP = (
    'pylib.local.device.local_device_environment.LocalDeviceEnvironment.SetUp'
)
_SUPER_INIT_DEVICES = (
    'pylib.local.device.local_device_environment.'
    'LocalDeviceEnvironment._InitDevices'
)


class TestLocalDeviceNetworkEnvironment(unittest.TestCase):
    def _create_env(self):
        args = mock.MagicMock()
        args.test_devices = [_DEVICE]
        with mock.patch(_SUPER_INIT, return_value=None):
            env = (
                local_device_network_environment.LocalDeviceNetworkEnvironment(
                    args, None, None
                )
            )
            env._device_serials = args.test_devices
            return env

    @mock.patch(_SUPER_INIT_DEVICES)
    @mock.patch(_SUPER_SETUP)
    @mock.patch('devil.android.sdk.adb_wrapper.AdbWrapper.Connect')
    def test_connect_success(
        self, mock_connect, mock_super_setup, mock_super_init_devices
    ):
        env = self._create_env()
        mock_connect.assert_not_called()

        env.SetUp()
        env._InitDevices()

        mock_connect.assert_called_once_with(_DEVICE)
        mock_super_setup.assert_called_once()
        mock_super_init_devices.assert_called_once()

    @mock.patch(_SUPER_SETUP)
    @mock.patch('devil.android.sdk.adb_wrapper.AdbWrapper.Connect')
    def test_connect_failure(self, mock_connect, mock_super_setup):
        for error in (
            device_errors.CommandTimeoutError('Connection timed out'),
            device_errors.CommandFailedError('unable to connect'),
        ):
            with self.subTest(error=error):
                env = self._create_env()
                mock_connect.side_effect = error
                with self.assertRaises(
                    device_errors.DeviceUnreachableError
                ) as ctx:
                    env.SetUp()
                self.assertTrue(ctx.exception.is_infra_error)
                mock_super_setup.assert_not_called()


if __name__ == '__main__':
    unittest.main()

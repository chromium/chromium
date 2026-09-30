# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from devil.android import device_errors
from devil.android.sdk import adb_wrapper

from pylib.local.device import local_device_environment


class LocalDeviceNetworkEnvironment(
    local_device_environment.LocalDeviceEnvironment
):
    """LocalDeviceEnvironment subclass for devices connected over TCP/IP."""

    def __init__(self, args, output_manager, error_func):
        super().__init__(args, output_manager, error_func)
        self._connected = False

    def _ConnectDevices(self):
        if self._connected:
            return
        for device in self._device_serials or []:
            logging.info('connecting to %s', device)
            try:
                adb_wrapper.AdbWrapper.Connect(device)
            except (
                device_errors.CommandTimeoutError,
                device_errors.CommandFailedError,
                device_errors.DeviceUnreachableError,
            ) as e:
                raise device_errors.DeviceUnreachableError(
                    f'Failed to connect to network device {device}: {e}',
                    is_infra_error=True,
                ) from e
        self._connected = True

    # override
    def SetUp(self):
        self._ConnectDevices()
        super().SetUp()

    # override
    def _InitDevices(self):
        self._ConnectDevices()
        super()._InitDevices()

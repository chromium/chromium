# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from devil.android.sdk import adb_wrapper

from pylib.local.device import local_device_environment


class LocalDeviceNetworkEnvironment(
    local_device_environment.LocalDeviceEnvironment):
  """A subclass of LocalDeviceEnvironment for devices connected over TCP/IP."""

  def __init__(self, args, output_manager, error_func):
    super().__init__(args, output_manager, error_func)
    logging.info('connecting to %s', args.test_devices[0])
    adb_wrapper.AdbWrapper.Connect(args.test_devices[0])

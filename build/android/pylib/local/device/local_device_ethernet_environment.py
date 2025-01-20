# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from devil.android.sdk import adb_wrapper

from pylib.local.device import local_device_environment


class LocalDeviceEthernetEnvironment(
    local_device_environment.LocalDeviceEnvironment):
  """
  A subclass of LocalDeviceEnvironment for devices connected over ethernet.

  This class cannot be instantiated. Subclasses should implement the
  GetDeviceHostname method, as this is specific to each environment.
  """

  def __init__(self, args, output_manager, error_func):
    super().__init__(args, output_manager, error_func)
    hostname = self.GetDeviceHostname()
    logging.info('connecting to %s', hostname)
    adb_wrapper.AdbWrapper.Connect(hostname)

  def GetDeviceHostname(self):
    raise NotImplementedError

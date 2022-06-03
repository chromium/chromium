# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging

from six.moves import range  # pylint: disable=redefined-builtin
from devil import base_error
from devil.android import device_errors
from devil.android import device_utils
from devil.utils import parallelizer
from devil.utils import reraiser_thread
from devil.utils import timeout_retry
from pylib.local.device import local_device_environment
from pylib.local.emulator import avd

# Mirroring https://bit.ly/2OjuxcS#23
_MAX_ANDROID_EMULATORS = 16


class LocalEmulatorEnvironment(local_device_environment.LocalDeviceEnvironment):

  def __init__(self, args, output_manager, error_func):
    super(LocalEmulatorEnvironment, self).__init__(args, output_manager,
                                                   error_func)
    self._avd_config = avd.AvdConfig(args.avd_config)
    if args.emulator_count < 1:
      error_func('--emulator-count must be >= 1')
    elif args.emulator_count >= _MAX_ANDROID_EMULATORS:
      logging.warning('--emulator-count capped at 16.')
    self._emulator_count = min(_MAX_ANDROID_EMULATORS, args.emulator_count)
    self._emulator_window = args.emulator_window
    self._writable_system = ((hasattr(args, 'use_webview_provider')
                              and args.use_webview_provider)
                             or (hasattr(args, 'replace_system_package')
                                 and args.replace_system_package)
                             or (hasattr(args, 'system_packages_to_remove')
                                 and args.system_packages_to_remove))

    self._emulator_instances = []
    self._device_serials = []

  #override
  def SetUp(self):
    self._avd_config.Install()

    emulator_instances = [
        self._avd_config.CreateInstance() for _ in range(self._emulator_count)
    ]

    def start_emulator_instance(e):

      def impl(e):
        try:
          e.Start(
              window=self._emulator_window,
              writable_system=self._writable_system)
        except avd.AvdException:
          logging.exception('Failed to start emulator instance.')
          return None
        try:
          device_utils.DeviceUtils(e.serial).WaitUntilFullyBooted()
        except base_error.BaseError:
          e.Stop()
          raise
        return e

      def retry_on_timeout(exc):
        return (isinstance(exc, device_errors.CommandTimeoutError)
                or isinstance(exc, reraiser_thread.TimeoutError))

      return timeout_retry.Run(
          impl,
          timeout=120 if self._writable_system else 30,
          retries=2,
          args=[e],
          retry_if_func=retry_on_timeout)

    parallel_emulators = parallelizer.SyncParallelizer(emulator_instances)
    self._emulator_instances = [
        emu
        for emu in parallel_emulators.pMap(start_emulator_instance).pGet(None)
        if emu is not None
    ]
    self._device_serials = [e.serial for e in self._emulator_instances]

    if not self._emulator_instances:
      raise Exception('Failed to start any instances of the emulator.')
    elif len(self._emulator_instances) < self._emulator_count:
      logging.warning(
          'Running with fewer emulator instances than requested (%d vs %d)',
          len(self._emulator_instances), self._emulator_count)

    super(LocalEmulatorEnvironment, self).SetUp()

  #override
  def TearDown(self):
    try:
      super(LocalEmulatorEnvironment, self).TearDown()
    finally:
      parallelizer.SyncParallelizer(self._emulator_instances).Stop()

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging

from devil.utils import parallelizer
from pylib.local.device import local_device_environment
from pylib.local.emulator import avd

from lib.proto import exception_recorder

# Mirroring https://bit.ly/2OjuxcS#23
_MAX_ANDROID_EMULATORS = 16


# TODO(crbug.com/40799394): After Telemetry is supported by python3 we can
# re-add super without arguments in this script.
# pylint: disable=super-with-arguments
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
    self._emulator_debug_tags = args.emulator_debug_tags
    self._emulator_enable_network = args.emulator_enable_network
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
        self._avd_config.CreateInstance(output_manager=self.output_manager)
        for _ in range(self._emulator_count)
    ]

    def start_emulator_instance(inst):
      try:
        inst.Start(window=self._emulator_window,
                   writable_system=self._writable_system,
                   debug_tags=self._emulator_debug_tags,
                   enable_network=self._emulator_enable_network,
                   require_fast_start=True,
                   retries=2)
      except avd.AvdStartException as e:
        exception_recorder.register(e)
        # The emulator is probably not responding so stop it forcely.
        logging.info("Force stop the emulator %s", inst)
        inst.Stop(force=True)
        raise
      except avd.AvdException as e:
        exception_recorder.register(e)
        logging.exception('Failed to start emulator instance.')
        return None
      return inst

    parallel_emulators = parallelizer.SyncParallelizer(emulator_instances)
    self._emulator_instances = [
        emu
        for emu in parallel_emulators.pMap(start_emulator_instance).pGet(None)
        if emu is not None
    ]
    self._device_serials = [e.serial for e in self._emulator_instances]

    if not self._emulator_instances:
      raise Exception('Failed to start any instances of the emulator.')
    if len(self._emulator_instances) < self._emulator_count:
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

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from pylib import constants
from pylib.local.device import local_device_environment
from pylib.local.machine import local_machine_environment

try:
  # local_emulator_environment depends on //tools.
  # If a client pulls in the //build subtree but not the //tools
  # one, fail at emulator environment creation time.
  from pylib.local.emulator import local_emulator_environment
except ImportError:
  local_emulator_environment = None


def CreateEnvironment(args, output_manager, error_func):

  if args.environment == 'local':
    if args.command not in constants.LOCAL_MACHINE_TESTS:
      if args.avd_config:
        if not local_emulator_environment:
          error_func('emulator environment requested but not available.')
        return local_emulator_environment.LocalEmulatorEnvironment(
            args, output_manager, error_func)
      return local_device_environment.LocalDeviceEnvironment(
          args, output_manager, error_func)
    else:
      return local_machine_environment.LocalMachineEnvironment(
          args, output_manager, error_func)

  error_func('Unable to create %s environment.' % args.environment)

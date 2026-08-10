# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib import constants
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_network_environment
from pylib.local.emulator import fuchsia_cuttlefish_emulator_environment
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
                    error_func(
                        'emulator environment requested but not available.'
                    )
                    raise RuntimeError('error_func must call exit inside.')
                return local_emulator_environment.LocalEmulatorEnvironment(
                    args, output_manager, error_func
                )

            # TODO(crbug.com/517946352): Fuchsia/Starview tests should explicitly pass
            # an emulator flag instead of inferring Cuttlefish from script existence
            # and environment variables.
            if (
                os.environ.get('ISOLATED_OUTDIR')
                or os.environ.get('CHROME_HEADLESS')
            ) and os.path.exists(
                fuchsia_cuttlefish_emulator_environment.CUTTLEFISH_SCRIPT
            ):
                return (
                    fuchsia_cuttlefish_emulator_environment.FuchsiaCuttlefishEmulatorEnvironment
                )(args, output_manager, error_func)
            if args.connect_over_network:
                return local_device_network_environment.LocalDeviceNetworkEnvironment(
                    args, output_manager, error_func
                )

            return local_device_environment.LocalDeviceEnvironment(
                args, output_manager, error_func
            )
        return local_machine_environment.LocalMachineEnvironment(
            args, output_manager, error_func
        )

    error_func('Unable to create %s environment.' % args.environment)
    raise RuntimeError('error_func must call exit inside.')

#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running tests E2E on a Fuchsia device."""

import argparse
import logging
import os
import sys
import tempfile

from contextlib import ExitStack
from typing import List

from common import register_common_args, register_device_args, \
                   register_log_args, resolve_packages
from compatible_utils import running_unattended
from ffx_integration import ScopedFfxConfig
from flash_device import register_update_args, update
from isolate_daemon import IsolateDaemon
from log_manager import LogManager, start_system_log
from publish_package import publish_packages, register_package_args
from run_blink_test import BlinkTestRunner
from run_executable_test import create_executable_test_runner, \
                                register_executable_test_args
from run_telemetry_test import TelemetryTestRunner
from run_webpage_test import WebpageTestRunner
from serve_repo import register_serve_args, serve_repository
from start_emulator import create_emulator_from_args, register_emulator_args
from test_connection import test_connection, test_device_connection
from test_runner import TestRunner


def _get_test_runner(runner_args: argparse.Namespace,
                     test_args: List[str]) -> TestRunner:
    """Initialize a suitable TestRunner class."""

    if runner_args.test_type == 'blink':
        return BlinkTestRunner(runner_args.out_dir, test_args,
                               runner_args.target_id)
    if runner_args.test_type in ['gpu', 'perf']:
        return TelemetryTestRunner(runner_args.test_type, runner_args.out_dir,
                                   test_args, runner_args.target_id)
    if runner_args.test_type in ['webpage']:
        return WebpageTestRunner(runner_args.out_dir, test_args,
                                 runner_args.target_id)
    return create_executable_test_runner(runner_args, test_args)


# pylint: disable=too-many-statements
def main():
    """E2E method for installing packages and running a test."""
    # Always add time stamps to the logs.
    logging.basicConfig(format='%(levelname)s %(asctime)s %(message)s')

    parser = argparse.ArgumentParser()
    parser.add_argument(
        'test_type',
        help='The type of test to run. Options include \'blink\', \'gpu\', '
        'or in the case of executable tests, the test name.')
    parser.add_argument('--device',
                        '-d',
                        action='store_true',
                        default=False,
                        help='Use an existing device.')
    parser.add_argument('--extra-path',
                        action='append',
                        help='Extra paths to append to the PATH environment')

    # Register arguments
    register_common_args(parser)
    register_device_args(parser)
    register_emulator_args(parser)
    register_executable_test_args(parser)
    register_update_args(parser, default_os_check='ignore')
    register_log_args(parser)
    register_package_args(parser, allow_temp_repo=True)
    register_serve_args(parser)

    # Treat unrecognized arguments as test specific arguments.
    runner_args, test_args = parser.parse_known_args()

    if not runner_args.out_dir:
        raise ValueError('--out-dir must be specified.')

    if runner_args.target_id:
        runner_args.device = True

    with ExitStack() as stack:
        log_manager = LogManager(runner_args.logs_dir)
        if running_unattended():
            if runner_args.extra_path:
                os.environ['PATH'] += os.pathsep + os.pathsep.join(
                    runner_args.extra_path)

            extra_inits = [log_manager]
            if runner_args.everlasting:
                # Setting the emu.instance_dir to match the named cache, so
                # we can keep these files across multiple runs.
                extra_inits.append(
                    ScopedFfxConfig(
                        'emu.instance_dir',
                        os.path.join(os.environ['HOME'],
                                     '.fuchsia_emulator/')))
            stack.enter_context(IsolateDaemon(extra_inits))
        else:
            if runner_args.logs_dir:
                logging.warning(
                    'You are using a --logs-dir, ensure the ffx '
                    'daemon is started with the logs.dir config '
                    'updated. We won\'t restart the daemon randomly'
                    ' anymore.')
            stack.enter_context(log_manager)

        if runner_args.device:
            update(runner_args.system_image_dir, runner_args.os_check,
                   runner_args.target_id, runner_args.serial_num)
            # Try to reboot the device if necessary since the ffx may ignore the
            # device state after the flash. See
            # https://cs.opensource.google/fuchsia/fuchsia/+/main:src/developer/ffx/lib/fastboot/src/common/fastboot.rs;drc=cfba0bdd4f8857adb6409f8ae9e35af52c0da93e;l=454
            test_device_connection(runner_args.target_id)
        else:
            runner_args.target_id = stack.enter_context(
                create_emulator_from_args(runner_args))
            test_connection(runner_args.target_id)

        test_runner = _get_test_runner(runner_args, test_args)
        package_deps = test_runner.package_deps

        if not runner_args.repo:
            # Create a directory that serves as a temporary repository.
            runner_args.repo = stack.enter_context(
                tempfile.TemporaryDirectory())

        publish_packages(package_deps.values(), runner_args.repo,
                         not runner_args.no_repo_init)

        stack.enter_context(serve_repository(runner_args))

        # Start system logging, after all possible restarts of the ffx daemon
        # so that logging will not be interrupted.
        start_system_log(log_manager, False, package_deps.values(),
                         ('--since', 'now'), runner_args.target_id)

        resolve_packages(package_deps.keys(), runner_args.target_id)
        return test_runner.run_test().returncode


if __name__ == '__main__':
    sys.exit(main())

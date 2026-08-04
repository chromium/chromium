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

import monitors

from common import has_ffx_isolate_dir, read_package_paths, \
                   register_common_args, register_device_args, \
                   register_log_args, \
                   resolve_packages
from compatible_utils import get_host_arch, running_unattended
from ffx_integration import ScopedFfxConfig
from flash_device import register_update_args, update
from isolate_daemon import IsolateDaemon
from log_manager import LogManager, start_system_log
from publish_package import ensure_repository, publish_packages, \
                            register_package_args
from run_blink_test import BlinkTestRunner
from run_executable_test import create_executable_test_runner, \
                                register_executable_test_args
from run_telemetry_test import TelemetryTestRunner
from run_webpage_test import WebpageTestRunner
from serve_repo import register_serve_args, serve_repository
from start_emulator import create_emulator_from_args, register_emulator_args
from orchestrate_runner import run_tests_with_orchestrate, support_orchestrate
from test_connection import test_connection, test_device_connection
from test_runner import TestRunner


def _get_test_runner(runner_args: argparse.Namespace,
                     test_args: List[str]) -> TestRunner:
    """Initialize a suitable TestRunner class."""

    if not runner_args.out_dir:
        raise ValueError('--out-dir must be specified.')

    if runner_args.test_type == 'blink':
        return BlinkTestRunner(runner_args.out_dir, test_args,
                               runner_args.target_id)
    if runner_args.test_type in ['gpu', 'perf']:
        return TelemetryTestRunner(runner_args.test_type, runner_args.out_dir,
                                   test_args, runner_args.target_id)
    if runner_args.test_type == 'webpage':
        return WebpageTestRunner(runner_args.out_dir, test_args,
                                 runner_args.target_id, runner_args.logs_dir)
    return create_executable_test_runner(runner_args, test_args)


# pylint: disable=too-many-statements,too-many-branches
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
    parser.add_argument('--orchestrate',
                        action='store_true',
                        default=False,
                        help='Use orchestrate to run tests.')

    # Register arguments
    register_common_args(parser)
    register_device_args(parser)
    register_emulator_args(parser)
    register_executable_test_args(parser)
    register_update_args(parser, default_os_check='ignore')
    register_log_args(parser)
    register_package_args(parser)
    register_serve_args(parser)

    # Treat unrecognized arguments as test specific arguments.
    runner_args, test_args = parser.parse_known_args()
    # Strip the '--' separator if it was captured in test_args, so we don't
    # pass it as a literal argument to the target test binary.
    if ['--'] == test_args[:1]:
        test_args.pop(0)

    runner_args.device = runner_args.device or bool(runner_args.target_id)

    use_orchestrate = (runner_args.orchestrate and not runner_args.device
                       and get_host_arch() == 'x64'
                       and support_orchestrate(runner_args.test_type))

    monitors.tag('fuchsia')
    with ExitStack() as stack, monitors.time_consumption(
            'orchestrate' if use_orchestrate else 'homemade', 'run'):
        if runner_args.logs_dir:
            # TODO(crbug.com/343242386): Find a way to upload metric output when
            # logs_dir is not defined.
            stack.push(lambda *_: monitors.dump(
                os.path.join(runner_args.logs_dir, 'invocations')))

        if use_orchestrate:
            target_cmd = [
                os.path.join(os.path.dirname(__file__),
                             'run_executable_test.py'),
                '--test-name', runner_args.test_type,
                '--out-dir', runner_args.out_dir
            ]
            if runner_args.logs_dir:
                target_cmd.extend(['--logs-dir', runner_args.logs_dir])
            if test_args:
                target_cmd.extend(test_args)

            packages = read_package_paths(runner_args.out_dir,
                                          runner_args.test_type)
            logging.info('Resolving package archives for \'%s\': %s',
                         runner_args.test_type, packages)
            return run_tests_with_orchestrate(
                runner_args.out_dir, packages, target_cmd,
                runner_args.logs_dir)
        if running_unattended():
            if not has_ffx_isolate_dir():
                stack.enter_context(IsolateDaemon(runner_args.logs_dir))

            if runner_args.everlasting:
                # Setting the emu.instance_dir to match the named cache, so
                # we can keep these files across multiple runs.
                # The configuration attaches to the isolate-dir, so it
                # needs to go after the IsolateDaemon.
                # There isn't a point of enabling the feature on devbox, it
                # won't use isolate-dir and the emu.instance_dir always goes to
                # the HOME directory.
                stack.enter_context(
                    ScopedFfxConfig(
                        'emu.instance_dir',
                        os.path.join(os.environ['HOME'],
                                     '.fuchsia_emulator/')))
        elif runner_args.logs_dir:
            logging.warning('You are using a --logs-dir, ensure ffx has '
                            'the logs.dir config updated.')
        log_manager = LogManager(runner_args.logs_dir,
                                 runner_args.wait_for_log_pattern)
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

        # Start system logging so that logging will not be interrupted.
        start_system_log(log_manager, False, package_deps.values(),
                         ('--since', 'now'), runner_args.target_id)

        if package_deps:
            if not runner_args.repo:
                # Create a directory that serves as a temporary repository.
                runner_args.repo = stack.enter_context(
                    tempfile.TemporaryDirectory())
                assert ensure_repository(runner_args), \
                    'Must initialize a repository with a temporary folder.'
                stack.enter_context(serve_repository(runner_args))
            publish_packages(package_deps.values(), runner_args)
            resolve_packages(package_deps.keys(), runner_args.target_id)
        elif runner_args.repo:
            # If there is a repo defined without packages, start the repo, so
            # that the following runs can use it.
            if ensure_repository(runner_args):
                stack.enter_context(serve_repository(runner_args))

        return test_runner.run_test().returncode


if __name__ == '__main__':
    sys.exit(main())

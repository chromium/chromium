#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running tests E2E on a Fuchsia device."""

import argparse
import sys
import tempfile

from contextlib import ExitStack
from typing import List

from common import register_common_args, register_device_args, \
                   register_log_args, resolve_packages, \
                   set_ffx_isolate_dir
from compatible_utils import running_unattended
from ffx_integration import ScopedFfxConfig, test_connection
from flash_device import register_update_args, update
from log_manager import LogManager, start_system_log
from publish_package import publish_packages, register_package_args
from run_blink_test import BlinkTestRunner
from run_executable_test import create_executable_test_runner, \
                                register_executable_test_args
from run_telemetry_test import TelemetryTestRunner
from run_webpage_test import WebpageTestRunner
from serve_repo import register_serve_args, serve_repository
from start_emulator import create_emulator_from_args, register_emulator_args
from test_runner import TestRunner
from ermine_ctl import ErmineCtl


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


def main():
    """E2E method for installing packages and running a test."""
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

    # Register arguments
    register_common_args(parser)
    register_device_args(parser)
    register_emulator_args(parser)
    register_executable_test_args(parser)
    register_update_args(parser, default_os_check='ignore', default_pave=True)
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
        if running_unattended():
            set_ffx_isolate_dir(
                stack.enter_context(tempfile.TemporaryDirectory()))
            stack.enter_context(
                ScopedFfxConfig('repository.server.listen', '"[::]:0"'))
        # crbug.com/1408189: overnet.cso causes flakes in overnet.
        stack.enter_context(ScopedFfxConfig('overnet.cso', 'disabled'))
        log_manager = stack.enter_context(LogManager(runner_args.logs_dir))
        if runner_args.device:
            update(runner_args.system_image_dir, runner_args.os_check,
                   runner_args.target_id, runner_args.serial_num,
                   runner_args.pave)
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

        ermine = ErmineCtl(runner_args.target_id)
        if ermine.exists:
            ermine.take_to_shell()

        resolve_packages(package_deps.keys(), runner_args.target_id)
        return test_runner.run_test().returncode


if __name__ == '__main__':
    sys.exit(main())

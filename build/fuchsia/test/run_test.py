#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running tests E2E on a Fuchsia device."""

import argparse
import sys
import tempfile

from contextlib import ExitStack
from typing import List

from common import register_common_args, register_device_args, \
                   register_log_args, resolve_packages, resolve_v1_packages
from ffx_integration import test_connection
from log_manager import LogManager, start_system_log
from publish_package import publish_packages, register_package_args
from run_blink_test import BlinkTestRunner
from run_executable_test import create_executable_test_runner
from run_gpu_test import GPUTestRunner
from serve_repo import register_serve_args, serve_repository
from start_emulator import create_emulator_from_args, register_emulator_args
from test_runner import TestRunner


def _get_test_runner(runner_args: argparse.Namespace,
                     test_args: List[str]) -> TestRunner:
    """Initialize a suitable TestRunner class."""

    if runner_args.test_type == 'blink':
        return BlinkTestRunner(runner_args.out_dir, test_args,
                               runner_args.target_id)
    if runner_args.test_type == 'gpu':
        return GPUTestRunner(runner_args.out_dir, test_args,
                             runner_args.target_id)
    return create_executable_test_runner(runner_args, test_args)


def main():
    """E2E method for installing packages and running a test."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'test_type',
        help='The type of test to run. Options include \'blink\', \'gpu\''
        'or in the case of gtests, the gtest name.')
    parser.add_argument('--device',
                        '-d',
                        action='store_true',
                        default=False,
                        help='Use an existing device.')

    # Register arguments
    register_common_args(parser)
    register_device_args(parser)
    register_emulator_args(parser)
    register_log_args(parser)
    register_package_args(parser, allow_temp_repo=True)
    register_serve_args(parser)

    # Treat unrecognized arguments as test specific arguments.
    runner_args, test_args = parser.parse_known_args()

    if not runner_args.out_dir:
        raise ValueError("--out-dir must be specified.")

    with ExitStack() as stack:
        log_manager = stack.enter_context(LogManager(runner_args.logs_dir))
        if not runner_args.device:
            if runner_args.target_id:
                raise ValueError(
                    'Target id can not be set without also setting \'-d\' flag.'
                )
            runner_args.target_id = stack.enter_context(
                create_emulator_from_args(runner_args))

        test_connection(runner_args.target_id)

        test_runner = _get_test_runner(runner_args, test_args)
        package_paths = test_runner.get_package_paths()

        # Start system logging.
        start_system_log(log_manager, False, package_paths, ('--since', 'now'),
                         runner_args.target_id)

        if not runner_args.repo:
            # Create a directory that serves as a temporary repository.
            runner_args.repo = stack.enter_context(
                tempfile.TemporaryDirectory())

        publish_packages(package_paths, runner_args.repo,
                         not runner_args.no_repo_init)

        with serve_repository(runner_args):

            # TODO(crbug.com/1342460): Remove when Telemetry and blink_web_tests
            # are using CFv2 packages.
            if runner_args.test_type in ['blink', 'gpu']:
                resolve_v1_packages(test_runner.packages,
                                    runner_args.target_id)
            else:
                resolve_packages(test_runner.packages, runner_args.target_id)
            return test_runner.run_test().returncode


if __name__ == '__main__':
    sys.exit(main())

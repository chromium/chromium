#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running tests E2E on a Fuchsia device."""

import argparse
import atexit
import shutil
import sys
import tempfile

from typing import List

import publish_package

from common import register_common_args
from run_blink_test import BlinkTestRunner
from run_executable_test import ExecutableTestRunner
from serve_repo import register_serve_args, run_serve_cmd
from test_runner import TestRunner


def get_test_runner(runner_args: argparse.Namespace,
                    test_args: List[str]) -> TestRunner:
    """Initialize a suitable TestRunner class."""
    if runner_args.test_type == 'blink':
        return BlinkTestRunner(runner_args.out_dir, test_args)
    return ExecutableTestRunner(runner_args.out_dir, test_args,
                                runner_args.test_type)


def main():
    """E2E method for installing packages and running a test."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'test_type',
        help='The type of test to run. Options include \'blink\''
        'or in the case of gtests, the gtest name.')

    # Register arguments
    register_common_args(parser)
    publish_package.register_package_args(parser, allow_temp_repo=True)
    register_serve_args(parser)

    # Treat unrecognized arguments as test specific arguments.
    runner_args, test_args = parser.parse_known_args()

    test_runner = get_test_runner(runner_args, test_args)
    packages = test_runner.get_package_paths()

    if not runner_args.repo:
        # Create a directory that serves as a temporary repository.
        tmpdir = tempfile.mkdtemp()
        atexit.register(shutil.rmtree, tmpdir)
        runner_args.repo = tmpdir

    publish_package.publish_packages(packages, runner_args.repo,
                                     not runner_args.no_repo_init)
    try:
        run_serve_cmd('start', runner_args)
        return test_runner.run_test().returncode
    finally:
        run_serve_cmd('stop', runner_args)


if __name__ == '__main__':
    sys.exit(main())

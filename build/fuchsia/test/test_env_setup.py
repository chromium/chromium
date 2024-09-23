#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a way to setup the test environment without running the tests. It
   covers starting up daemon, starting up emulator or flashing physical device,
   setting up fuchsia package repository, publishing the packages and resolving
   the packages on the target.

   It shares most of the logic with the regular run_test.py, but overriding its
   _get_test_runner to wait_for_sigterm instead of running tests. So it's
   blocking until being killed.

   Since the setup_env does not run the tests, caller should use the pid file to
   detect when the environment is ready.

   Killing the process running the setup_env() function would tear down the
   test environment."""

import argparse
import os
import sys

from subprocess import CompletedProcess
from typing import List

import run_test
from common import catch_sigterm, wait_for_sigterm
from test_runner import TestRunner


class _Blocker(TestRunner):
    """A TestRunner implementation to block the process until sigterm is
    received."""

    # private, use run_tests.get_test_runner function instead.
    def __init__(self, out_dir: str, target_id: str, package_deps: List[str],
                 pid_file: str):
        super().__init__(out_dir, [], [], target_id, package_deps)
        self.pid_file = pid_file

    def run_test(self) -> CompletedProcess:
        open(self.pid_file, 'w').close()
        try:
            wait_for_sigterm()
            return CompletedProcess(args='', returncode=0)
        finally:
            os.remove(self.pid_file)


def setup_env(mypid: int = 0) -> int:
    """Sets up the environment and blocks until sigterm is received.

       Args:
           mypid: The script creates the file at logs-dir/$mypid.pid when the
                  environment is ready.
                  Since this script won't run tests directly, caller can use
                  this file to decide when to start running the tests."""
    catch_sigterm()

    if mypid == 0:
        mypid = os.getpid()

    # The 'setup-environment' is a place holder and has no specific meaning; the
    # run_test._get_test_runner is overridden.
    sys.argv.append('setup-environment')

    def get_test_runner(runner_args: argparse.Namespace, *_) -> TestRunner:
        return _Blocker(
            runner_args.out_dir, runner_args.target_id, runner_args.packages,
            os.path.join(runner_args.logs_dir,
                         'test_env_setup.' + str(mypid) + '.pid'))

    # pylint: disable=protected-access
    run_test._get_test_runner = get_test_runner
    return run_test.main()


if __name__ == '__main__':
    sys.exit(setup_env())

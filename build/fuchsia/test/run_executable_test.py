#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for standalone CFv2 test executables."""

import argparse
import sys

from typing import List

from common import REPO_ALIAS, register_common_args, resolve_packages, \
                   run_ffx_command
from test_runner import TestRunner


class ExecutableTestRunner(TestRunner):
    """Test runner for running standalone test executables."""

    def __init__(self, out_dir: str, test_args: List[str],
                 test_name: str) -> None:
        self._test_name = test_name
        super().__init__(out_dir, test_args)

    def _get_packages(self):
        return [self._test_name]

    def run_test(self):
        resolve_packages(self.packages)
        test_cmd = [
            'test',
            'run',
            f'fuchsia-pkg://{REPO_ALIAS}/{self._test_name}#meta/' \
            f'{self._test_name}.cm',
        ]
        if self._test_args:
            test_cmd.append('--')
            test_cmd.extend(self._test_args)
        return run_ffx_command(test_cmd)


def register_gtest_args(parser: argparse.ArgumentParser) -> None:
    """Register common arguments for GtestRunner."""
    test_args = parser.add_argument_group('test', 'arguments for test running')
    test_args.add_argument('--test-name',
                           help='Name of the test package (e.g. unit_tests).')


def main():
    """Stand-alone function for running gtests."""
    parser = argparse.ArgumentParser()
    register_gtest_args(parser)
    register_common_args(parser)
    runner_args, test_args = parser.parse_known_args()
    runner = ExecutableTestRunner(runner_args.out_dir, runner_args.test_name,
                                  test_args)
    return runner.run_test()


if __name__ == '__main__':
    sys.exit(main())

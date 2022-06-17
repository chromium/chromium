#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for standalone CFv2 test executables."""

import argparse
import logging
import os
import shutil
import subprocess
import sys

from typing import List, Optional

from common import get_component_uri, register_common_args, \
                   register_device_args, resolve_packages, run_ffx_command
from ffx_integration import FfxTestRunner
from test_runner import TestRunner


def _copy_custom_output_file(test_runner: FfxTestRunner, file: str,
                             dest: str) -> None:
    """Copy custom test output file from the device to the host."""
    artifact_dir = test_runner.get_custom_artifact_directory()
    if not artifact_dir:
        logging.error(
            'Failed to parse custom artifact directory from test summary '
            'output files. Not copying %s from the device', file)
        return
    shutil.copy(os.path.join(artifact_dir, file), dest)


class ExecutableTestRunner(TestRunner):
    """Test runner for running standalone test executables."""

    def __init__(self, out_dir: str, test_args: List[str], test_name: str,
                 target_id: Optional[str]) -> None:
        super().__init__(out_dir, test_args, [test_name], target_id)
        self._test_name = test_name
        self._custom_artifact_directory = None
        self._isolated_script_test_output = None
        self._test_launcher_summary_output = None

    def _get_args(self) -> List[str]:
        if not self._test_args:
            return []
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--isolated-script-test-output',
            help='If present, store test results on this path.')
        # This argument has been deprecated.
        parser.add_argument('--isolated-script-test-perf-output',
                            help=argparse.SUPPRESS)
        parser.add_argument(
            '--test-launcher-shard-index',
            type=int,
            default=os.environ.get('GTEST_SHARD_INDEX'),
            help='Index of this instance amongst swarming shards.')
        parser.add_argument(
            '--test-launcher-summary-output',
            help='Where the test launcher will output its json.')
        parser.add_argument(
            '--test-launcher-total-shards',
            type=int,
            default=os.environ.get('GTEST_TOTAL_SHARDS'),
            help='Total number of swarming shards of this suite.')

        args, child_args = parser.parse_known_args(self._test_args)
        if args.isolated_script_test_output:
            self._isolated_script_test_output = args.isolated_script_test_output
            child_args.append(
                '--isolated-script-test-output=/custom_artifacts/%s' %
                os.path.basename(self._isolated_script_test_output))
        if args.test_launcher_shard_index:
            child_args.append('--test-launcher-shard-index=%d' %
                              args.test_launcher_shard_index)
        if args.test_launcher_total_shards:
            child_args.append([
                '--test-launcher-total-shards=%d' %
                args.test_launcher_total_shards
            ])
        if args.test_launcher_summary_output:
            self._test_launcher_summary_output = \
                args.test_launcher_summary_output
            child_args.append(
                '--test-launcher-summary-output=/custom_artifacts/%s' %
                os.path.basename(self._test_launcher_summary_output))
        return child_args

    def _postprocess(self, test_runner: FfxTestRunner) -> None:
        if self._test_launcher_summary_output:
            _copy_custom_output_file(
                test_runner,
                os.path.basename(self._test_launcher_summary_output),
                self._test_launcher_summary_output)
        if self._isolated_script_test_output:
            _copy_custom_output_file(
                test_runner,
                os.path.basename(self._test_launcher_summary_output),
                self._isolated_script_test_output)

    def run_test(self) -> subprocess.Popen:
        resolve_packages(self.packages, self._target_id)
        test_args = self._get_args()
        with FfxTestRunner() as test_runner:
            test_proc = test_runner.run_test(
                get_component_uri(self._test_name), test_args, self._target_id)

            # Symbolize output from test process and print to terminal.
            symbolize_cmd = ['debug', 'symbolize', '--']
            for pkg_path in self._package_paths:
                symbol_path = os.path.join(os.path.dirname(pkg_path),
                                           'ids.txt')
                symbolize_cmd.extend(('--ids-txt', symbol_path))
            run_ffx_command(symbolize_cmd,
                            stdin=test_proc.stdout,
                            stdout=sys.stdout,
                            stderr=subprocess.STDOUT)
            if test_proc.wait() == 0:
                logging.info('Process exited normally with status code 0.')
            else:
                # The test runner returns an error status code if *any*
                # tests fail, so we should proceed anyway.
                logging.warning('Process exited with status code %d.',
                                test_proc.returncode)
            self._postprocess(test_runner)
        return test_proc


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
    register_device_args(parser)
    runner_args, test_args = parser.parse_known_args()
    runner = ExecutableTestRunner(runner_args.out_dir, runner_args.test_name,
                                  test_args, runner_args.target_id)
    return runner.run_test()


if __name__ == '__main__':
    sys.exit(main())

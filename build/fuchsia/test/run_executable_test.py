#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
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

from common import get_component_uri, get_host_arch, \
                   register_common_args, register_device_args, \
                   register_log_args
from compatible_utils import map_filter_file_to_package_file
from ffx_integration import FfxTestRunner, run_symbolizer
from test_runner import TestRunner

DEFAULT_TEST_SERVER_CONCURRENCY = 4


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


def _copy_coverage_files(test_runner: FfxTestRunner, dest: str) -> None:
    """Copy debug data file from the device to the host if it exists."""

    coverage_dir = test_runner.get_debug_data_directory()
    if not coverage_dir:
        logging.info(
            'Failed to parse coverage data directory from test summary '
            'output files. Not copying coverage files from the device.')
        return
    shutil.copytree(coverage_dir, dest, dirs_exist_ok=True)


# pylint: disable=too-many-instance-attributes
class ExecutableTestRunner(TestRunner):
    """Test runner for running standalone test executables."""

    def __init__(  # pylint: disable=too-many-arguments
            self, out_dir: str, test_args: List[str], test_name: str,
            target_id: Optional[str], code_coverage_dir: str,
            logs_dir: Optional[str], package_deps: List[str],
            test_realm: Optional[str]) -> None:
        super().__init__(out_dir, test_args, [test_name], target_id,
                         package_deps)
        if not self._test_args:
            self._test_args = []
        self._test_name = test_name
        self._code_coverage_dir = code_coverage_dir
        self._custom_artifact_directory = None
        self._isolated_script_test_output = None
        self._isolated_script_test_perf_output = None
        self._logs_dir = logs_dir
        self._test_launcher_summary_output = None
        self._test_server = None
        self._test_realm = test_realm

    def _get_args(self) -> List[str]:
        parser = argparse.ArgumentParser()
        parser.add_argument(
            '--isolated-script-test-output',
            help='If present, store test results on this path.')
        parser.add_argument('--isolated-script-test-perf-output',
                            help='If present, store chartjson results on this '
                            'path.')
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
        parser.add_argument(
            '--test-launcher-filter-file',
            help='Filter file(s) passed to target test process. Use ";" to '
            'separate multiple filter files.')
        parser.add_argument('--test-launcher-jobs',
                            type=int,
                            help='Sets the number of parallel test jobs.')
        parser.add_argument('--enable-test-server',
                            action='store_true',
                            default=False,
                            help='Enable Chrome test server spawner.')
        parser.add_argument('--test-arg',
                            dest='test_args',
                            action='append',
                            help='Legacy flag to pass in arguments for '
                            'the test process. These arguments can now be '
                            'passed in without a preceding "--" flag.')
        args, child_args = parser.parse_known_args(self._test_args)
        if args.isolated_script_test_output:
            self._isolated_script_test_output = args.isolated_script_test_output
            child_args.append(
                '--isolated-script-test-output=/custom_artifacts/%s' %
                os.path.basename(self._isolated_script_test_output))
        if args.isolated_script_test_perf_output:
            self._isolated_script_test_perf_output = \
                args.isolated_script_test_perf_output
            child_args.append(
                '--isolated-script-test-perf-output=/custom_artifacts/%s' %
                os.path.basename(self._isolated_script_test_perf_output))
        if args.test_launcher_shard_index is not None:
            child_args.append('--test-launcher-shard-index=%d' %
                              args.test_launcher_shard_index)
        if args.test_launcher_total_shards is not None:
            child_args.append('--test-launcher-total-shards=%d' %
                              args.test_launcher_total_shards)
        if args.test_launcher_summary_output:
            self._test_launcher_summary_output = \
                args.test_launcher_summary_output
            child_args.append(
                '--test-launcher-summary-output=/custom_artifacts/%s' %
                os.path.basename(self._test_launcher_summary_output))
        if args.test_launcher_filter_file:
            test_launcher_filter_files = map(
                map_filter_file_to_package_file,
                args.test_launcher_filter_file.split(';'))
            child_args.append('--test-launcher-filter-file=' +
                              ';'.join(test_launcher_filter_files))
        if args.test_launcher_jobs is not None:
            test_concurrency = args.test_launcher_jobs
        else:
            test_concurrency = DEFAULT_TEST_SERVER_CONCURRENCY
        if args.enable_test_server:
            # Repos other than chromium may not have chrome_test_server_spawner,
            # and they may not run server at all, so only import the test_server
            # when it's really necessary.

            # pylint: disable=import-outside-toplevel
            from test_server import setup_test_server
            # pylint: enable=import-outside-toplevel
            self._test_server, spawner_url_base = setup_test_server(
                self._target_id, test_concurrency)
            child_args.append('--remote-test-server-spawner-url-base=%s' %
                              spawner_url_base)
        if get_host_arch() == 'x64':
            # TODO(crbug.com/40202294) Remove once Vulkan is enabled by
            # default.
            child_args.append('--use-vulkan=native')
        else:
            # TODO(crbug.com/42050042, crbug.com/42050537) Remove swiftshader
            # once the vulkan is enabled by default.
            child_args.extend(
                ['--use-vulkan=swiftshader', '--ozone-platform=headless'])
        if args.test_args:
            child_args.extend(args.test_args)
        return child_args

    def _postprocess(self, test_runner: FfxTestRunner) -> None:
        if self._test_server:
            self._test_server.Stop()
        if self._test_launcher_summary_output:
            _copy_custom_output_file(
                test_runner,
                os.path.basename(self._test_launcher_summary_output),
                self._test_launcher_summary_output)
        if self._isolated_script_test_output:
            _copy_custom_output_file(
                test_runner,
                os.path.basename(self._isolated_script_test_output),
                self._isolated_script_test_output)
        if self._isolated_script_test_perf_output:
            _copy_custom_output_file(
                test_runner,
                os.path.basename(self._isolated_script_test_perf_output),
                self._isolated_script_test_perf_output)
        if self._code_coverage_dir:
            _copy_coverage_files(test_runner,
                                 os.path.basename(self._code_coverage_dir))

    def run_test(self) -> subprocess.Popen:
        test_args = self._get_args()
        with FfxTestRunner(self._logs_dir) as test_runner:
            test_proc = test_runner.run_test(
                get_component_uri(self._test_name), test_args, self._target_id,
                self._test_realm)

            symbol_paths = []
            for pkg_path in self.package_deps.values():
                symbol_paths.append(
                    os.path.join(os.path.dirname(pkg_path), 'ids.txt'))
            # Symbolize output from test process and print to terminal.
            symbolizer_proc = run_symbolizer(symbol_paths, test_proc.stdout,
                                             sys.stdout)
            symbolizer_proc.communicate()

            if test_proc.wait() == 0:
                logging.info('Process exited normally with status code 0.')
            else:
                # The test runner returns an error status code if *any*
                # tests fail, so we should proceed anyway.
                logging.warning('Process exited with status code %d.',
                                test_proc.returncode)
            self._postprocess(test_runner)
        return test_proc


def create_executable_test_runner(runner_args: argparse.Namespace,
                                  test_args: List[str]):
    """Helper for creating an ExecutableTestRunner."""

    return ExecutableTestRunner(runner_args.out_dir, test_args,
                                runner_args.test_type, runner_args.target_id,
                                runner_args.code_coverage_dir,
                                runner_args.logs_dir, runner_args.package_deps,
                                runner_args.test_realm)


def register_executable_test_args(parser: argparse.ArgumentParser) -> None:
    """Register common arguments for ExecutableTestRunner."""

    test_args = parser.add_argument_group('test', 'arguments for test running')
    test_args.add_argument('--code-coverage-dir',
                           default=None,
                           help='Directory to place code coverage '
                           'information. Only relevant when the target was '
                           'built with |fuchsia_code_coverage| set to true.')
    test_args.add_argument('--test-name',
                           dest='test_type',
                           help='Name of the test package (e.g. '
                           'unit_tests).')
    test_args.add_argument(
        '--test-realm',
        default=None,
        help='The realm to run the test in. This field is optional and takes '
        'the form: /path/to/realm:test_collection. See '
        'https://fuchsia.dev/go/components/non-hermetic-tests')
    test_args.add_argument('--package-deps',
                           action='append',
                           help='A list of the full path of the dependencies '
                           'to retrieve the symbol ids. Keeping it empty to '
                           'automatically generates from package_metadata.')


def main():
    """Stand-alone function for running executable tests."""

    parser = argparse.ArgumentParser()
    register_common_args(parser)
    register_device_args(parser)
    register_log_args(parser)
    register_executable_test_args(parser)
    runner_args, test_args = parser.parse_known_args()
    runner = create_executable_test_runner(runner_args, test_args)
    return runner.run_test().returncode


if __name__ == '__main__':
    sys.exit(main())

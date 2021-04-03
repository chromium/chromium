#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Deploys and runs a test package on a Fuchsia target."""

import argparse
import os
import runner_logs
import sys

from common_args import AddCommonArgs, AddTargetSpecificArgs, \
                        ConfigureLogging, GetDeploymentTargetForArgs
from net_test_server import SetupTestServer
from run_test_package import RunTestPackage, RunTestPackageArgs, SystemLogReader
from runner_exceptions import HandleExceptionAndReturnExitCode
from runner_logs import RunnerLogManager
from symbolizer import BuildIdsPaths

DEFAULT_TEST_SERVER_CONCURRENCY = 4

TEST_DATA_DIR = '/tmp'
TEST_FILTER_PATH = TEST_DATA_DIR + '/test_filter.txt'
TEST_LLVM_PROFILE_PATH = TEST_DATA_DIR + '/llvm-profile'
TEST_PERF_RESULT_PATH = TEST_DATA_DIR + '/test_perf_summary.json'
TEST_RESULT_PATH = TEST_DATA_DIR + '/test_summary.json'

TEST_REALM_NAME = 'chromium_tests'


def AddTestExecutionArgs(arg_parser):
  test_args = arg_parser.add_argument_group('testing',
                                            'Test execution arguments')
  test_args.add_argument('--gtest_filter',
                         help='GTest filter to use in place of any default.')
  test_args.add_argument(
      '--gtest_repeat',
      help='GTest repeat value to use. This also disables the '
      'test launcher timeout.')
  test_args.add_argument(
      '--test-launcher-retry-limit',
      help='Number of times that test suite will retry failing '
      'tests. This is multiplicative with --gtest_repeat.')
  test_args.add_argument('--test-launcher-shard-index',
                         type=int,
                         default=os.environ.get('GTEST_SHARD_INDEX'),
                         help='Index of this instance amongst swarming shards.')
  test_args.add_argument('--test-launcher-total-shards',
                         type=int,
                         default=os.environ.get('GTEST_TOTAL_SHARDS'),
                         help='Total number of swarming shards of this suite.')
  test_args.add_argument('--gtest_break_on_failure',
                         action='store_true',
                         default=False,
                         help='Should GTest break on failure; useful with '
                         '--gtest_repeat.')
  test_args.add_argument('--single-process-tests',
                         action='store_true',
                         default=False,
                         help='Runs the tests and the launcher in the same '
                         'process. Useful for debugging.')
  test_args.add_argument('--test-launcher-batch-limit',
                         type=int,
                         help='Sets the limit of test batch to run in a single '
                         'process.')
  # --test-launcher-filter-file is specified relative to --out-dir,
  # so specifying type=os.path.* will break it.
  test_args.add_argument(
      '--test-launcher-filter-file',
      default=None,
      help='Override default filter file passed to target test '
      'process. Set an empty path to disable filtering.')
  test_args.add_argument('--test-launcher-jobs',
                         type=int,
                         help='Sets the number of parallel test jobs.')
  test_args.add_argument('--test-launcher-summary-output',
                         help='Where the test launcher will output its json.')
  test_args.add_argument('--enable-test-server',
                         action='store_true',
                         default=False,
                         help='Enable Chrome test server spawner.')
  test_args.add_argument(
      '--test-launcher-bot-mode',
      action='store_true',
      default=False,
      help='Informs the TestLauncher to that it should enable '
      'special allowances for running on a test bot.')
  test_args.add_argument('--isolated-script-test-output',
                         help='If present, store test results on this path.')
  test_args.add_argument(
      '--isolated-script-test-perf-output',
      help='If present, store chartjson results on this path.')
  test_args.add_argument('--use-run-test-component',
                         default=False,
                         action='store_true',
                         help='Run the test package hermetically using '
                         'run-test-component, rather than run.')
  test_args.add_argument(
      '--code-coverage',
      default=False,
      action='store_true',
      help='Gather code coverage information and place it in '
      'the output directory.')
  test_args.add_argument('--code-coverage-dir',
                         default=os.getcwd(),
                         help='Directory to place code coverage information. '
                         'Only relevant when --code-coverage set to true. '
                         'Defaults to current directory.')
  test_args.add_argument('--child-arg',
                         action='append',
                         help='Arguments for the test process.')
  test_args.add_argument('child_args',
                         nargs='*',
                         help='Arguments for the test process.')


def main():
  parser = argparse.ArgumentParser()
  AddTestExecutionArgs(parser)
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  args = parser.parse_args()

  # Flag out_dir is required for tests launched with this script.
  if not args.out_dir:
    raise ValueError("out-dir must be specified.")

  # Code coverage uses runtests, which calls run_test_component.
  if args.code_coverage:
    args.use_run_test_component = True

  ConfigureLogging(args)

  child_args = []
  if args.test_launcher_shard_index != None:
    child_args.append(
        '--test-launcher-shard-index=%d' % args.test_launcher_shard_index)
  if args.test_launcher_total_shards != None:
    child_args.append(
        '--test-launcher-total-shards=%d' % args.test_launcher_total_shards)
  if args.single_process_tests:
    child_args.append('--single-process-tests')
  if args.test_launcher_bot_mode:
    child_args.append('--test-launcher-bot-mode')
  if args.test_launcher_batch_limit:
    child_args.append('--test-launcher-batch-limit=%d' %
                       args.test_launcher_batch_limit)

  # Only set --test-launcher-jobs if the caller specifies it, in general.
  # If the caller enables the test-server then we need to launch the right
  # number of instances to match the maximum number of parallel test jobs, so
  # in that case we set --test-launcher-jobs based on the number of CPU cores
  # specified for the emulator to use.
  test_concurrency = None
  if args.test_launcher_jobs:
    test_concurrency = args.test_launcher_jobs
  elif args.enable_test_server:
    if args.device == 'device':
      test_concurrency = DEFAULT_TEST_SERVER_CONCURRENCY
    else:
      test_concurrency = args.cpu_cores
  if test_concurrency:
    child_args.append('--test-launcher-jobs=%d' % test_concurrency)

  if args.gtest_filter:
    child_args.append('--gtest_filter=' + args.gtest_filter)
  if args.gtest_repeat:
    child_args.append('--gtest_repeat=' + args.gtest_repeat)
    child_args.append('--test-launcher-timeout=-1')
  if args.test_launcher_retry_limit:
    child_args.append(
        '--test-launcher-retry-limit=' + args.test_launcher_retry_limit)
  if args.gtest_break_on_failure:
    child_args.append('--gtest_break_on_failure')
  if args.test_launcher_summary_output:
    child_args.append('--test-launcher-summary-output=' + TEST_RESULT_PATH)
  if args.isolated_script_test_output:
    child_args.append('--isolated-script-test-output=' + TEST_RESULT_PATH)
  if args.isolated_script_test_perf_output:
    child_args.append('--isolated-script-test-perf-output=' +
                      TEST_PERF_RESULT_PATH)

  if args.child_arg:
    child_args.extend(args.child_arg)
  if args.child_args:
    child_args.extend(args.child_args)

  test_realms = []
  if args.use_run_test_component:
    test_realms = [TEST_REALM_NAME]

  try:
    with GetDeploymentTargetForArgs(args) as target, \
         SystemLogReader() as system_logger, \
         RunnerLogManager(args.runner_logs_dir, BuildIdsPaths(args.package)):
      target.Start()

      if args.system_log_file and args.system_log_file != '-':
        system_logger.Start(target, args.package, args.system_log_file)

      if args.test_launcher_filter_file:
        target.PutFile(args.test_launcher_filter_file,
                       TEST_FILTER_PATH,
                       for_package=args.package_name,
                       for_realms=test_realms)
        child_args.append('--test-launcher-filter-file=' + TEST_FILTER_PATH)

      test_server = None
      if args.enable_test_server:
        assert test_concurrency
        test_server = SetupTestServer(target, test_concurrency,
                                      args.package_name, test_realms)

      run_package_args = RunTestPackageArgs.FromCommonArgs(args)
      if args.use_run_test_component:
        run_package_args.test_realm_label = TEST_REALM_NAME
        run_package_args.use_run_test_component = True
      returncode = RunTestPackage(args.out_dir, target, args.package,
                                  args.package_name, child_args,
                                  run_package_args)

      if test_server:
        test_server.Stop()

      if args.code_coverage:
        # Copy all the files in the profile directory. /* is used instead
        # of recursively copying due to permission issues for the latter.
        target.GetFile(TEST_LLVM_PROFILE_PATH + '/*', args.code_coverage_dir)

      if args.test_launcher_summary_output:
        target.GetFile(TEST_RESULT_PATH,
                       args.test_launcher_summary_output,
                       for_package=args.package_name,
                       for_realms=test_realms)

      if args.isolated_script_test_output:
        target.GetFile(TEST_RESULT_PATH,
                       args.isolated_script_test_output,
                       for_package=args.package_name,
                       for_realms=test_realms)

      if args.isolated_script_test_perf_output:
        target.GetFile(TEST_PERF_RESULT_PATH,
                       args.isolated_script_test_perf_output,
                       for_package=args.package_name,
                       for_realms=test_realms)

      return returncode

  except:
    return HandleExceptionAndReturnExitCode()


if __name__ == '__main__':
  sys.exit(main())

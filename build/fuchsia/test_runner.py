#!/usr/bin/env vpython3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Deploys and runs a test package on a Fuchsia target."""

import argparse
import logging
import os
import shutil
import sys

import ffx_session
from common_args import AddCommonArgs, AddTargetSpecificArgs, \
                        ConfigureLogging, GetDeploymentTargetForArgs
from net_test_server import SetupTestServer
from run_test_package import RunTestPackage
from runner_exceptions import HandleExceptionAndReturnExitCode

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from compatible_utils import map_filter_file_to_package_file

DEFAULT_TEST_SERVER_CONCURRENCY = 4

TEST_LLVM_PROFILE_DIR = 'llvm-profile'
TEST_PERF_RESULT_FILE = 'test_perf_summary.json'
TEST_RESULT_FILE = 'test_summary.json'

TEST_REALM_NAME = 'chromium_tests'


class CustomArtifactsTestOutputs():
  """A TestOutputs implementation for CFv2 tests, where tests emit files into
  /custom_artifacts that are retrieved from the device automatically via ffx."""

  def __init__(self, target):
    super(CustomArtifactsTestOutputs, self).__init__()
    self._target = target
    self._ffx_session_context = ffx_session.FfxSession(target._log_manager)
    self._ffx_session = None

  def __enter__(self):
    self._ffx_session = self._ffx_session_context.__enter__()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self._ffx_session = None
    self._ffx_session_context.__exit__(exc_type, exc_val, exc_tb)
    return False

  def GetFfxSession(self):
    assert self._ffx_session
    return self._ffx_session

  def GetDevicePath(self, path):
    return '/custom_artifacts/' + path

  def GetOutputDirectory(self):
    return self._ffx_session.get_output_dir()

  def GetFile(self, glob, destination):
    """Places all files/directories matched by a glob into a destination."""
    directory = self._ffx_session.get_custom_artifact_directory()
    if not directory:
      logging.error(
          'Failed to parse custom artifact directory from test summary output '
          'files. Not copying %s from the device', glob)
      return
    shutil.copy(os.path.join(directory, glob), destination)

  def GetCoverageProfiles(self, destination):
    directory = self._ffx_session.get_debug_data_directory()
    if not directory:
      logging.error(
          'Failed to parse debug data directory from test summary output '
          'files. Not copying coverage profiles from the device')
      return
    coverage_dir = os.path.join(directory, TEST_LLVM_PROFILE_DIR)
    shutil.copytree(coverage_dir, destination, dirs_exist_ok=True)


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
  test_args.add_argument('--test-launcher-print-test-stdio',
                         choices=['auto', 'always', 'never'],
                         help='Controls when full test output is printed.'
                         'auto means to print it when the test failed.')
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
      help='Filter file(s) passed to target test process. Use ";" to separate '
      'multiple filter files ')
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
  test_args.add_argument('--gtest_also_run_disabled_tests',
                         default=False,
                         action='store_true',
                         help='Run tests prefixed with DISABLED_')
  test_args.add_argument('--test-arg',
                         dest='test_args',
                         action='append',
                         help='Argument for the test process.')
  test_args.add_argument('child_args',
                         nargs='*',
                         help='Arguments for the test process.')
  test_args.add_argument('--use-vulkan',
                         help='\'native\', \'swiftshader\' or \'none\'.')


def main():
  parser = argparse.ArgumentParser()
  AddTestExecutionArgs(parser)
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  args = parser.parse_args()

  # Flag out_dir is required for tests launched with this script.
  if not args.out_dir:
    raise ValueError("out-dir must be specified.")

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
  if args.test_launcher_print_test_stdio:
    child_args.append('--test-launcher-print-test-stdio=%s' %
                      args.test_launcher_print_test_stdio)

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
  if args.gtest_also_run_disabled_tests:
    child_args.append('--gtest_also_run_disabled_tests')
  if args.test_args:
    child_args.extend(args.test_args)

  if args.child_args:
    child_args.extend(args.child_args)

  if args.use_vulkan:
    child_args.append('--use-vulkan=' + args.use_vulkan)
  elif args.target_cpu == 'x64':
    # TODO(crbug.com/1261646) Remove once Vulkan is enabled by default.
    child_args.append('--use-vulkan=native')
  else:
    # Use swiftshader on arm64 by default because most arm64 bots currently
    # don't support Vulkan emulation.
    child_args.append('--use-vulkan=swiftshader')
    child_args.append('--ozone-platform=headless')

  try:
    with GetDeploymentTargetForArgs(args) as target, \
         CustomArtifactsTestOutputs(target) as test_outputs:
      if args.test_launcher_summary_output:
        child_args.append('--test-launcher-summary-output=' +
                          test_outputs.GetDevicePath(TEST_RESULT_FILE))
      if args.isolated_script_test_output:
        child_args.append('--isolated-script-test-output=' +
                          test_outputs.GetDevicePath(TEST_RESULT_FILE))
      if args.isolated_script_test_perf_output:
        child_args.append('--isolated-script-test-perf-output=' +
                          test_outputs.GetDevicePath(TEST_PERF_RESULT_FILE))

      target.Start()
      target.StartSystemLog(args.package)

      if args.test_launcher_filter_file:
        # TODO(crbug.com/1279803): Until one can send file to the device when
        # running a test, filter files must be read from the test package.
        test_launcher_filter_files = map(
            map_filter_file_to_package_file,
            args.test_launcher_filter_file.split(';'))
        child_args.append('--test-launcher-filter-file=' +
                          ';'.join(test_launcher_filter_files))

      test_server = None
      if args.enable_test_server:
        assert test_concurrency
        (test_server,
         spawner_url_base) = SetupTestServer(target, test_concurrency)
        child_args.append('--remote-test-server-spawner-url-base=' +
                          spawner_url_base)

      returncode = RunTestPackage(target, test_outputs.GetFfxSession(),
                                  args.package, args.package_name, child_args)

      if test_server:
        test_server.Stop()

      if args.code_coverage:
        test_outputs.GetCoverageProfiles(args.code_coverage_dir)

      if args.test_launcher_summary_output:
        test_outputs.GetFile(TEST_RESULT_FILE,
                             args.test_launcher_summary_output)

      if args.isolated_script_test_output:
        test_outputs.GetFile(TEST_RESULT_FILE, args.isolated_script_test_output)

      if args.isolated_script_test_perf_output:
        test_outputs.GetFile(TEST_PERF_RESULT_FILE,
                             args.isolated_script_test_perf_output)

      return returncode

  except:
    return HandleExceptionAndReturnExitCode()


if __name__ == '__main__':
  sys.exit(main())

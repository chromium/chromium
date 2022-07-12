#!/usr/bin/env vpython3
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the CTS test APKs stored in CIPD."""

import argparse
import contextlib
import json
import logging
import os
import shutil
import sys
import tempfile
import zipfile

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'build', 'android'))
# pylint: disable=wrong-import-position,import-error
import devil_chromium  # pylint: disable=unused-import
from devil.android.ndk import abis
from devil.android.sdk import version_codes
from devil.android.tools import script_common
from devil.utils import cmd_helper
from devil.utils import logging_common
from pylib.local.emulator import avd
from pylib.utils import test_filter

# cts test archives for all platforms are stored in this bucket
# contents need to be updated if there is an important fix to any of
# the tests

_APP_MODE_FULL = 'full'
_APP_MODE_INSTANT = 'instant'

_TEST_RUNNER_PATH = os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir,
    'build', 'android', 'test_runner.py')

_WEBVIEW_CTS_GCS_PATH_FILE = os.path.join(
    os.path.dirname(__file__), 'cts_config', 'webview_cts_gcs_path.json')
_ARCH_SPECIFIC_CTS_INFO = ["filename", "unzip_dir", "_origin"]

_CTS_ARCHIVE_DIR = os.path.join(os.path.dirname(__file__), 'cts_archive')

_CTS_WEBKIT_PACKAGES = ["com.android.cts.webkit", "android.webkit.cts"]

_TEST_APK_AS_INSTANT_ARG = '--test-apk-as-instant'

SDK_PLATFORM_DICT = {
    version_codes.MARSHMALLOW: 'M',
    version_codes.NOUGAT: 'N',
    version_codes.NOUGAT_MR1: 'N',
    version_codes.OREO: 'O',
    version_codes.OREO_MR1: 'O',
    version_codes.PIE: 'P',
    version_codes.Q: 'Q',
    version_codes.R: 'R',
    version_codes.S: 'S',
    version_codes.S_V2: 'S',
}

# The test apks are apparently compatible across all architectures, the
# arm vs x86 split is to match the current cts releases and in case things
# start to diverge in the future.  Keeping the arm64 (instead of arm) dict
# key to avoid breaking the bots that specify --arch arm64 to invoke the tests.
_SUPPORTED_ARCH_DICT = {
    # TODO(aluo): Investigate how to force WebView abi on platforms supporting
    # multiple abis.
    # The test apks under 'arm64' support both arm and arm64 devices.
    abis.ARM: 'arm64',
    abis.ARM_64: 'arm64',
    # The test apks under 'x86' support both x86 and x86_64 devices.
    abis.X86: 'x86',
    abis.X86_64: 'x86'
}


TEST_FILTER_OPT = '--test-filter'

def GetCtsInfo(arch, cts_release, item):
  """Gets contents of CTS Info for arch and cts_release.

  See _WEBVIEW_CTS_GCS_PATH_FILE
  """
  with open(_WEBVIEW_CTS_GCS_PATH_FILE) as f:
    cts_gcs_path_info = json.load(f)
  try:
    if item in _ARCH_SPECIFIC_CTS_INFO:
      return cts_gcs_path_info[cts_release]['arch'][arch][item]
    return cts_gcs_path_info[cts_release][item]
  except KeyError:
    # pylint: disable=raise-missing-from
    # This script is executed with python2, and cannot use 'from'.
    raise Exception('No %s info available for arch:%s, android:%s' %
                    (item, arch, cts_release))


def GetCTSModuleNames(arch, cts_release):
  """Gets the module apk name of the arch and cts_release"""
  test_runs = GetCtsInfo(arch, cts_release, 'test_runs')
  return [os.path.basename(r['apk']) for r in test_runs]


def GetTestRunFilterArg(args, test_run, test_app_mode=None, arch=None):
  """ Merges json file filters with cmdline filters using
      test_filter.InitializeFilterFromArgs
  """

  test_app_mode = test_app_mode or _APP_MODE_FULL

  # Convert cmdline filters to test-filter style
  filter_string = test_filter.InitializeFilterFromArgs(args)

  # Get all the filters for either include or exclude patterns
  # and filter where an architecture is provided and does not match
  # The architecture is used when tests only fail on one architecture
  def getTestRunFilters(key):
    filters = test_run.get(key, [])
    return [
        filter_["match"] for filter_ in filters
        if 'arch' not in filter_ or filter_['arch'] == arch
        if 'mode' not in filter_ or filter_['mode'] == test_app_mode
    ]

  # Only add inclusion filters if there's not already one specified, since
  # they would conflict, see test_filter.ConflictingPositiveFiltersException.
  if not test_filter.HasPositivePatterns(filter_string):
    patterns = getTestRunFilters("includes")
    filter_string = test_filter.AppendPatternsToFilter(
        filter_string, positive_patterns=patterns)

  if args.skip_expected_failures:
    patterns = getTestRunFilters("excludes")
    filter_string = test_filter.AppendPatternsToFilter(
        filter_string, negative_patterns=patterns)

  if filter_string:
    return [TEST_FILTER_OPT + '=' + filter_string]
  return []


def RunCTS(
    test_runner_args,
    local_cts_dir,
    apk,
    *,  # Optional parameters must be passed by keyword (PEP 3102)
    voice_service=None,
    additional_apks=None,
    test_app_mode=None,
    json_results_file=None):
  """Run tests in apk using test_runner script at _TEST_RUNNER_PATH.

  Returns the script result code, test results will be stored in
  the json_results_file file if specified.
  """

  test_app_mode = test_app_mode or _APP_MODE_FULL

  local_test_runner_args = test_runner_args + ['--test-apk',
                                               os.path.join(local_cts_dir, apk)]

  if voice_service:
    local_test_runner_args += ['--use-voice-interaction-service', voice_service]

  if additional_apks:
    for additional_apk in additional_apks:
      additional_apk_tmp = os.path.join(local_cts_dir, additional_apk['apk'])
      local_test_runner_args += ['--additional-apk', additional_apk_tmp]

      if additional_apk.get('forced_queryable', False):
        local_test_runner_args += [
            '--forced-queryable-additional-apk', additional_apk_tmp
        ]

      if test_app_mode == _APP_MODE_INSTANT and not additional_apk.get(
          'force_full_mode', False):
        local_test_runner_args += [
            '--instant-additional-apk', additional_apk_tmp
        ]

  if json_results_file:
    local_test_runner_args += ['--json-results-file=%s' %
                               json_results_file]

  return cmd_helper.RunCmd(
      [_TEST_RUNNER_PATH, 'instrumentation'] + local_test_runner_args)


def MergeTestResults(existing_results_json, additional_results_json):
  """Appends results in additional_results_json to existing_results_json."""
  for k, v in additional_results_json.items():
    if k not in existing_results_json:
      existing_results_json[k] = v
    else:
      if isinstance(v, dict):
        if not isinstance(existing_results_json[k], dict):
          raise NotImplementedError(
              "Can't merge results field %s of different types" % v)
        existing_results_json[k].update(v)
      elif isinstance(v, list):
        if not isinstance(existing_results_json[k], list):
          raise NotImplementedError(
              "Can't merge results field %s of different types" % v)
        existing_results_json[k].extend(v)
      else:
        raise NotImplementedError(
            "Can't merge results field %s that is not a list or dict" % v)


def ExtractCTSZip(args, arch, cts_release):
  """Extract the CTS tests for cts_release.

  Extract the CTS zip file from _CTS_ARCHIVE_DIR to
  apk_dir if specified, or a new temporary directory if not.
  Returns following tuple (local_cts_dir, base_cts_dir, delete_cts_dir):
    local_cts_dir - CTS extraction location for current arch and cts_release
    base_cts_dir - Root directory for all the arches and platforms
    delete_cts_dir - Set if the base_cts_dir was created as a temporary
    directory
  """
  base_cts_dir = None
  delete_cts_dir = False
  relative_cts_zip_path = GetCtsInfo(arch, cts_release, 'filename')

  if args.apk_dir:
    base_cts_dir = args.apk_dir
  else:
    base_cts_dir = tempfile.mkdtemp()
    delete_cts_dir = True

  cts_zip_path = os.path.join(_CTS_ARCHIVE_DIR, relative_cts_zip_path)
  local_cts_dir = os.path.join(base_cts_dir,
                               GetCtsInfo(arch, cts_release,
                                          'unzip_dir')
                              )
  zf = zipfile.ZipFile(cts_zip_path, 'r')
  zf.extractall(local_cts_dir)
  return (local_cts_dir, base_cts_dir, delete_cts_dir)


def RunAllCTSTests(args, arch, cts_release, test_runner_args):
  """Run CTS tests downloaded from _CTS_BUCKET.

  Downloads CTS tests from bucket, runs them for the
  specified cts_release+arch, then creates a single
  results json file (if specified)
  Returns 0 if all tests passed, otherwise
  returns the failure code of the last failing
  test.
  """
  local_cts_dir, base_cts_dir, delete_cts_dir = ExtractCTSZip(args, arch,
                                                              cts_release)
  cts_result = 0
  json_results_file = args.json_results_file
  try:
    cts_test_runs = GetCtsInfo(arch, cts_release, 'test_runs')
    cts_results_json = {}
    for cts_test_run in cts_test_runs:
      iteration_cts_result = 0

      test_apk = cts_test_run['apk']
      voice_service = cts_test_run.get('voice_service')
      # Some tests need additional APKs that providing mocking
      # services to run
      additional_apks = cts_test_run.get('additional_apks')

      test_app_mode = (_APP_MODE_INSTANT
                       if args.test_apk_as_instant else _APP_MODE_FULL)

      # If --module-apk is specified then skip tests in all other modules
      if args.module_apk and os.path.basename(test_apk) != args.module_apk:
        continue

      iter_test_runner_args = test_runner_args + GetTestRunFilterArg(
          args, cts_test_run, test_app_mode, arch)

      if json_results_file:
        with tempfile.NamedTemporaryFile() as iteration_json_file:
          iteration_cts_result = RunCTS(
              test_runner_args=iter_test_runner_args,
              local_cts_dir=local_cts_dir,
              apk=test_apk,
              voice_service=voice_service,
              additional_apks=additional_apks,
              test_app_mode=test_app_mode,
              json_results_file=iteration_json_file.name)
          with open(iteration_json_file.name) as f:
            additional_results_json = json.load(f)
            MergeTestResults(cts_results_json, additional_results_json)
      else:
        iteration_cts_result = RunCTS(test_runner_args=iter_test_runner_args,
                                      local_cts_dir=local_cts_dir,
                                      apk=test_apk,
                                      voice_service=voice_service,
                                      additional_apks=additional_apks,
                                      test_app_mode=test_app_mode)
      if iteration_cts_result:
        cts_result = iteration_cts_result
    if json_results_file:
      with open(json_results_file, 'w') as f:
        json.dump(cts_results_json, f, indent=2)
  finally:
    if delete_cts_dir and base_cts_dir:
      shutil.rmtree(base_cts_dir)

  return cts_result


def DetermineCtsRelease(device):
  """Determines the CTS release based on the Android SDK level

  Args:
    device: The DeviceUtils instance
  Returns:
    The first letter of the cts_release in uppercase.
  Raises:
    Exception: if we don't have the CTS tests for the device platform saved in
      CIPD already.
  """
  cts_release = SDK_PLATFORM_DICT.get(device.build_version_sdk)
  if not cts_release:
    # Check if we're above the supported version range.
    max_supported_sdk = max(SDK_PLATFORM_DICT.keys())
    if device.build_version_sdk > max_supported_sdk:
      raise Exception("We don't have tests for API level {api_level}, try "
                      "running the {release} tests with `--cts-release "
                      "{release}`".format(
                          api_level=device.build_version_sdk,
                          release=SDK_PLATFORM_DICT.get(max_supported_sdk),
                      ))
    # Otherwise, we must be below the supported version range.
    min_supported_sdk = min(SDK_PLATFORM_DICT.keys())
    raise Exception("We don't support running CTS tests on platforms less "
                    "than {release}".format(
                        release=SDK_PLATFORM_DICT.get(min_supported_sdk), ))
  logging.info(('Using test APKs from CTS release=%s because '
                'build.version.sdk=%s'),
               cts_release, device.build_version_sdk)
  return cts_release


def DetermineArch(device):
  """Determines which architecture to use based on the device properties

  Args:
    device: The DeviceUtils instance
  Returns:
    The formatted arch string (as expected by CIPD)
  Raises:
    Exception: if device architecture is not currently supported by this script.
  """
  arch = _SUPPORTED_ARCH_DICT.get(device.product_cpu_abi)
  if not arch:
    raise Exception('Could not find CIPD bucket for your device arch (' +
                    device.product_cpu_abi +
                    '), please specify with --arch')
  logging.info('Guessing arch=%s because product.cpu.abi=%s', arch,
               device.product_cpu_abi)
  return arch


def UninstallAnyCtsWebkitPackages(device):
  for package in _CTS_WEBKIT_PACKAGES:
    device.Uninstall(package)


def ForwardArgsToTestRunner(known_args):
  """Convert any args that should be forwarded to test_runner.py"""
  forwarded_args = []
  if known_args.devices:
    # test_runner.py parses --device as nargs instead of append args
    forwarded_args.extend(['--device'] + known_args.devices)
  if known_args.denylist_file:
    forwarded_args.extend(['--denylist-file', known_args.denylist_file])
  if known_args.test_apk_as_instant:
    forwarded_args.extend([_TEST_APK_AS_INSTANT_ARG])
  if known_args.verbose:
    forwarded_args.extend(['-' + 'v' * known_args.verbose])
  #TODO: Pass quiet to test runner when it becomes supported
  return forwarded_args


@contextlib.contextmanager
def GetDevice(args):
  try:
    emulator_instance = None
    if args.avd_config:
      avd_config = avd.AvdConfig(args.avd_config)
      avd_config.Install()
      emulator_instance = avd_config.CreateInstance()
      # Start the emulator w/ -writable-system s.t. we can remount the system
      # partition r/w and install our own webview provider.
      emulator_instance.Start(writable_system=True)

    devices = script_common.GetDevices(args.devices, args.denylist_file)
    device = devices[0]
    if len(devices) > 1:
      logging.warning('Detection of arch and cts-release will use 1st of %d '
                      'devices: %s', len(devices), device.serial)
    yield device
  finally:
    if emulator_instance:
      emulator_instance.Stop()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--arch',
      choices=list(set(_SUPPORTED_ARCH_DICT.values())),
      default=None,
      type=str,
      help=('Architecture to for CTS tests. Will auto-determine based on '
            'the device ro.product.cpu.abi property.'))
  parser.add_argument(
      '--cts-release',
      # TODO(aluo): --platform is deprecated (the meaning is unclear).
      '--platform',
      choices=sorted(set(SDK_PLATFORM_DICT.values())),
      required=False,
      default=None,
      help='Which CTS release to use for the run. This should generally be <= '
           'device OS level (otherwise, the newer tests will fail). If '
           'unspecified, the script will auto-determine the release based on '
           'device OS level.')
  parser.add_argument(
      '--skip-expected-failures',
      action='store_true',
      help="Option to skip all tests that are expected to fail.  Can't be used "
           "with test filters.")
  parser.add_argument(
      '--apk-dir',
      help='Directory to extract CTS APKs to. '
           'Will use temp directory by default.')
  parser.add_argument(
      '--test-launcher-summary-output',
      '--json-results-file',
      '--write-full-results-to',
      '--isolated-script-test-output',
      dest='json_results_file', type=os.path.realpath,
      help='If set, will dump results in JSON form to the specified file. '
           'Note that this will also trigger saving per-test logcats to '
           'logdog.')
  parser.add_argument(
      '-m',
      '--module-apk',
      dest='module_apk',
      help='CTS module apk name in ' + _WEBVIEW_CTS_GCS_PATH_FILE +
      ' file, without the path prefix.')
  parser.add_argument(
      '--avd-config',
      type=os.path.realpath,
      help='Path to the avd config textpb. '
           '(See //tools/android/avd/proto for message definition'
           ' and existing textpb files.)')
  # We are re-using this argument that is used by our test runner
  # to detect if we are testing against an instant app
  # This allows us to know if we should filter tests based off the app
  # app mode or not
  # We are adding this filter directly instead of calling a function like
  # we do with test_filter.AddFilterOptions below because this would make
  # test-apk a required argument which would make this script fail
  # because we provide this later programmatically in this script
  parser.add_argument(
      _TEST_APK_AS_INSTANT_ARG,
      action='store_true',
      help='Run CTS tests in instant app mode. '
      'Instant apps run in a more restrictive execution environment.')


  test_filter.AddFilterOptions(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args, test_runner_args = parser.parse_known_args()
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize()

  test_runner_args.extend(ForwardArgsToTestRunner(args))

  with GetDevice(args) as device:
    arch = args.arch or DetermineArch(device)
    cts_release = args.cts_release or DetermineCtsRelease(device)

    if (args.test_filter_files or args.test_filter
        or args.isolated_script_test_filter):
      # TODO(aluo): auto-determine the module based on the test filter and the
      # available tests in each module
      if not args.module_apk:
        args.module_apk = 'CtsWebkitTestCases.apk'

    platform_modules = GetCTSModuleNames(arch, cts_release)
    if args.module_apk and args.module_apk not in platform_modules:
      raise Exception('--module-apk for arch==' + arch + 'and cts_release=='
                      + cts_release + ' must be one of: '
                      + ', '.join(platform_modules))

    # Need to uninstall all previous cts webkit packages so that the
    # MockContentProvider names won't conflict with a previously installed
    # one under a different package name.  This is due to CtsWebkitTestCases's
    # package name change from M to N versions of the tests while keeping the
    # MockContentProvider's authority string the same.
    UninstallAnyCtsWebkitPackages(device)

    return RunAllCTSTests(args, arch, cts_release, test_runner_args)


if __name__ == '__main__':
  sys.exit(main())

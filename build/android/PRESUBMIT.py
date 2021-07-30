# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for android buildbot.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  build_android_dir = input_api.PresubmitLocalPath()

  def J(*dirs):
    """Returns a path relative to presubmit directory."""
    return input_api.os_path.join(build_android_dir, *dirs)

  build_pys = [
      r'gn/.*\.py$',
      r'gyp/.*\.py$',
  ]
  tests = []
  # yapf likes formatting the extra_paths_list to be less readable.
  # yapf: disable
  tests.extend(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          pylintrc='pylintrc',
          # Temporarily disabled until pylint-2.6: crbug.com/1100664
          disabled_warnings=[
              'no-member',
              'superfluous-parens',
              'no-name-in-module',
              'import-error'],
          files_to_skip=[
              r'.*_pb2\.py',
              # The following are all temporary due to: crbug.com/1100664
              r'.*list_java_targets\.py',
              r'.*fast_local_dev_server\.py',
              r'.*incremental_javac_test_android_library.py',
          ] + build_pys,
          extra_paths_list=[
              J(),
              J('gyp'),
              J('buildbot'),
              J('..', 'util', 'lib', 'common'),
              J('..', '..', 'third_party', 'catapult', 'common',
                'py_trace_event'),
              J('..', '..', 'third_party', 'catapult', 'common', 'py_utils'),
              J('..', '..', 'third_party', 'catapult', 'devil'),
              J('..', '..', 'third_party', 'catapult', 'tracing'),
              J('..', '..', 'third_party', 'depot_tools'),
              J('..', '..', 'third_party', 'colorama', 'src'),
              J('..', '..', 'build'),
          ]))
  tests.extend(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          files_to_check=build_pys,
          files_to_skip=[
              r'.*_pb2\.py',
              r'.*_pb2\.py',
          ],
          extra_paths_list=[J('gyp'), J('gn')]))
  # yapf: enable

  # Disabled due to http://crbug.com/410936
  #output.extend(input_api.canned_checks.RunUnitTestsInDirectory(
  #input_api, output_api, J('buildbot', 'tests')))

  pylib_test_env = dict(input_api.environ)
  pylib_test_env.update({
      'PYTHONPATH': build_android_dir,
      'PYTHONDONTWRITEBYTECODE': '1',
  })
  tests.extend(
      input_api.canned_checks.GetUnitTests(
          input_api,
          output_api,
          unit_tests=[
              J('.', 'convert_dex_profile_tests.py'),
              J('.', 'emma_coverage_stats_test.py'),
              J('.', 'list_class_verification_failures_test.py'),
              J('pylib', 'constants', 'host_paths_unittest.py'),
              J('pylib', 'gtest', 'gtest_test_instance_test.py'),
              J('pylib', 'instrumentation',
                'instrumentation_test_instance_test.py'),
              J('pylib', 'local', 'device', 'local_device_gtest_run_test.py'),
              J('pylib', 'local', 'device',
                'local_device_instrumentation_test_run_test.py'),
              J('pylib', 'local', 'device', 'local_device_test_run_test.py'),
              J('pylib', 'local', 'machine',
                'local_machine_junit_test_run_test.py'),
              J('pylib', 'output', 'local_output_manager_test.py'),
              J('pylib', 'output', 'noop_output_manager_test.py'),
              J('pylib', 'output', 'remote_output_manager_test.py'),
              J('pylib', 'results', 'json_results_test.py'),
              J('pylib', 'symbols', 'apk_native_libs_unittest.py'),
              J('pylib', 'symbols', 'elf_symbolizer_unittest.py'),
              J('pylib', 'symbols', 'symbol_utils_unittest.py'),
              J('pylib', 'utils', 'chrome_proxy_utils_test.py'),
              J('pylib', 'utils', 'decorators_test.py'),
              J('pylib', 'utils', 'device_dependencies_test.py'),
              J('pylib', 'utils', 'dexdump_test.py'),
              J('pylib', 'utils', 'gold_utils_test.py'),
              J('pylib', 'utils', 'proguard_test.py'),
              J('pylib', 'utils', 'test_filter_test.py'),
              J('gyp', 'util', 'build_utils_test.py'),
              J('gyp', 'util', 'manifest_utils_test.py'),
              J('gyp', 'util', 'md5_check_test.py'),
              J('gyp', 'util', 'resource_utils_test.py'),
          ],
          env=pylib_test_env,
          run_on_python2=False,
          run_on_python3=True,
          skip_shebang_check=True))

  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

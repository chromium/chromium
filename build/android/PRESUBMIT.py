# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check android presubmit requirements."""


def GetScopedUnitTests(input_api, build_android_dir, *, is_upload):
    """Returns the list of unit test paths to execute for the current change.

    Scopes tests based on affected subdirectories under build/android/.
    If PRESUBMIT.py or PRESUBMIT_test.py is modified, or if no diffs are
    available, returns all unit tests.
    """

    def J(*dirs):
        """Returns an absolute path under the presubmit directory."""
        return input_api.os_path.join(build_android_dir, *dirs)

    gyp_tests = [
        J('gyp', 'compile_java_tests.py'),
        J('gyp', 'create_unwind_table_tests.py'),
        J('gyp', 'dex_test.py'),
        J('gyp', 'gcc_preprocess_tests.py'),
        J('gyp', 'java_cpp_enum_tests.py'),
        J('gyp', 'java_cpp_features_tests.py'),
        J('gyp', 'java_cpp_strings_tests.py'),
        J('gyp', 'java_google_api_keys_tests.py'),
        J('gyp', 'util', 'java_cpp_utils_test.py'),
        J('gyp', 'util', 'manifest_utils_test.py'),
        J('gyp', 'util', 'md5_check_test.py'),
        J('gyp', 'util', 'resource_utils_test.py'),
    ]
    pylib_tests = [
        J('pylib', 'base', 'output_manager_test_case.py'),
        J('pylib', 'constants', 'host_paths_unittest.py'),
        J('pylib', 'gtest', 'gtest_test_instance_test.py'),
        J('pylib', 'instrumentation', 'instrumentation_parser_test.py'),
        J('pylib', 'instrumentation', 'instrumentation_test_instance_test.py'),
        J('pylib', 'local', 'device', 'local_device_gtest_run_test.py'),
        J(
            'pylib',
            'local',
            'device',
            'local_device_instrumentation_test_run_test.py',
        ),
        J('pylib', 'local', 'device', 'local_device_test_run_test.py'),
        J('pylib', 'local', 'emulator', 'avd_test.py'),
        J('pylib', 'local', 'emulator', 'ini_test.py'),
        J('pylib', 'local', 'machine', 'local_machine_junit_test_run_test.py'),
        J('pylib', 'output', 'local_output_manager_test.py'),
        J('pylib', 'output', 'noop_output_manager_test.py'),
        J('pylib', 'output', 'remote_output_manager_test.py'),
        J(
            'pylib',
            'results',
            'flakiness_dashboard',
            'json_results_generator_unittest.py',
        ),
        J('pylib', 'results', 'json_results_test.py'),
        J('pylib', 'utils', 'code_coverage_utils_test.py'),
        J('pylib', 'utils', 'device_dependencies_test.py'),
        J('pylib', 'utils', 'dexdump_test.py'),
        J('pylib', 'utils', 'gold_utils_test.py'),
        J('pylib', 'utils', 'test_filter_test.py'),
    ]
    root_tests = [
        J('convert_dex_profile_tests.py'),
        J('list_class_verification_failures_test.py'),
        J('test_runner_test.py'),
    ]
    # Build server tests are flaky on bots.
    # Tracking bug: crbug.com/559199069
    if is_upload:
        root_tests.append(J('fast_local_dev_server_test.py'))

    all_tests = gyp_tests + pylib_tests + root_tests

    # When running full presubmit without diffs, run the entire test suite.
    if input_api.no_diffs:
        return all_tests

    affected_files = {
        input_api.os_path.normpath(f.AbsoluteLocalPath())
        for f in input_api.AffectedFiles(include_deletes=True)
    }

    # If this presubmit or its test was modified, run all tests.
    if {J('PRESUBMIT.py'), J('PRESUBMIT_test.py')} & affected_files:
        return all_tests

    root_dir = input_api.os_path.normpath(build_android_dir)

    def dir_has_python_changes(subdir=None):
        if subdir:
            prefix = (
                input_api.os_path.join(root_dir, subdir) + input_api.os_path.sep
            )
        else:
            prefix = root_dir + input_api.os_path.sep
        for path in affected_files:
            if not path.endswith('.py') and not path.endswith('.json'):
                continue
            if not path.startswith(prefix):
                continue
            # If checking root dir (subdir=None), exclude subdirectories.
            if subdir is None:
                rel = path[len(prefix) :]
                if input_api.os_path.sep in rel:
                    continue
            return True
        return False

    selected_tests = []

    # Matches gyp build scripts and dependencies.
    if dir_has_python_changes('gyp'):
        selected_tests.extend(gyp_tests)

    # Matches pylib test runner libraries and dependencies.
    if dir_has_python_changes('pylib'):
        selected_tests.extend(pylib_tests)

    # Matches root build/android/ Python scripts and tools.
    if dir_has_python_changes(None):
        selected_tests.extend(root_tests)

    # Include any test file modified directly that is in all_tests.
    for test in all_tests:
        if test in affected_files and test not in selected_tests:
            selected_tests.append(test)

    return selected_tests


def CommonChecks(input_api, output_api, *, is_upload):
    # These tools don't run on Windows so these tests don't work and give many
    # verbose and cryptic failure messages. Linting the code is also skipped on
    # Windows because it will fail due to os differences.
    if input_api.sys.platform == 'win32':
        return []

    build_android_dir = input_api.PresubmitLocalPath()

    def J(*dirs):
        """Returns an absolute path under the presubmit directory."""
        return input_api.os_path.join(build_android_dir, *dirs)

    build_pys = [
        r'gyp/.*\.py$',
        r'.*create_unwind_table\.py',
        r'.*create_unwind_table_tests\.py',
    ]
    tests = []
    # yapf likes formatting the extra_paths_list to be less readable.
    # yapf: disable
    tests.extend(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          pylintrc='pylintrc-3.2',
          files_to_skip=[
              r'.*_pb2\.py'
          ] + build_pys,
          extra_paths_list=[
              J(),
              J('gyp'),
              J('buildbot'),
              J('..'),
              J('..', 'util'),
              J('..', '..', 'third_party', 'catapult', 'common',
                'py_trace_event'),
              J('..', '..', 'third_party', 'catapult', 'common', 'py_utils'),
              J('..', '..', 'third_party', 'catapult', 'devil'),
              J('..', '..', 'third_party', 'catapult', 'tracing'),
              J('..', '..', 'third_party', 'depot_tools'),
              J('..', '..', 'third_party', 'colorama', 'src'),
          ],
          version='3.2'))
    tests.extend(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          pylintrc='pylintrc-3.2',
          files_to_check=build_pys,
          files_to_skip=[
              r'.*_pb2\.py',
              r'.*_pb2\.py',
              r'.*create_unwind_table\.py',
              r'.*create_unwind_table_tests\.py',
          ],
          extra_paths_list=[
              J(),
              J('..'),
              J('..', 'gn_ast'),
              J('..', 'util'),
              J('..', '..', 'google_apis'),
              J('..', '..', 'third_party'),
              J('gyp'),
              J('gyp', 'util'),
          ],
          version='3.2'))
    # yapf: enable

    pylib_test_env = dict(input_api.environ)
    pylib_test_env.update(
        {
            'PYTHONPATH': build_android_dir,
            'PYTHONDONTWRITEBYTECODE': '1',
        }
    )

    pytests = GetScopedUnitTests(
        input_api, build_android_dir, is_upload=is_upload
    )
    if pytests:
        tests.extend(
            input_api.canned_checks.GetUnitTests(
                input_api, output_api, unit_tests=pytests, env=pylib_test_env
            )
        )

    return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api, is_upload=True)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api, is_upload=False)

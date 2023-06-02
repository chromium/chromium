# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit for android_webview/tools."""

def _GetPythonUnitTests(input_api, output_api):
  return input_api.canned_checks.GetUnitTestsRecursively(
      input_api,
      output_api,
      input_api.PresubmitLocalPath(),
      files_to_check=['.*_test\\.py$'],
      files_to_skip=[])


def CommonChecks(input_api, output_api):
  """Presubmit checks run on both upload and commit.
  """
  checks = []

  src_root = input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..')
  checks.extend(
      input_api.canned_checks.GetPylint(
          input_api,
          output_api,
          pylintrc='pylintrc',
          disabled_warnings=[
              'R0801',  # suppress pylint duplicate code false positive
          ],
          # Allows pylint to find dependencies imported by scripts in this
          # directory.
          extra_paths_list=[
              input_api.os_path.join(src_root, 'build', 'android'),
              input_api.os_path.join(src_root, 'build', 'android', 'gyp'),
              input_api.os_path.join(src_root, 'third_party', 'catapult',
                                     'common', 'py_utils'),
              input_api.os_path.join(src_root, 'third_party', 'catapult',
                                     'devil'),
          ],
          version='2.7'))
  checks.extend(_GetPythonUnitTests(input_api, output_api))
  return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks on CL upload."""
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks on commit."""
  return CommonChecks(input_api, output_api)

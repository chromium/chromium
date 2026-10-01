# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit for android_webview/tools."""

PRESUBMIT_VERSION = '2.0.0'


def _GetPythonUnitTests(input_api, output_api):
  return input_api.canned_checks.GetUnitTestsRecursively(
    input_api,
    output_api,
    input_api.PresubmitLocalPath(),
    files_to_check=['.*_test\\.py$'],
    files_to_skip=[],
  )


def CommonChecks(input_api, output_api):
  """Presubmit checks run on both upload and commit."""
  if not input_api.HasAffectedFiles(extensions=('.py', '.json')):
    return []
  checks = []
  checks.extend(_GetPythonUnitTests(input_api, output_api))
  return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks on CL upload."""
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks on commit."""
  return CommonChecks(input_api, output_api)


def CheckPatchFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
    bypass_warnings=False,
  )


def CheckRuff(input_api, output_api):
  return input_api.RunTests(
    input_api.canned_checks.GetRuff(input_api, output_api)
  )

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
"""Presubmit for build/util"""


def _GetBlacklist(input_api):
  blacklist = []
  affected_files = input_api.change.AffectedFiles()
  version_script_change = next(
      (f for f in affected_files
       if re.search('\\/version\\.py$|\\/version_test\\.py$', f.LocalPath())),
      None)

  if version_script_change is None:
    blacklist.append('version_test\\.py$')

  android_chrome_version_script_change = next(
      (f for f in affected_files if re.search(
          '\\/android_chrome_version\\.py$|'
          '\\/android_chrome_version_test\\.py$', f.LocalPath())), None)

  if android_chrome_version_script_change is None:
    blacklist.append('android_chrome_version_test\\.py$')

  return blacklist


def _GetPythonUnitTests(input_api, output_api):
  # No need to test if files are unchanged
  blacklist = _GetBlacklist(input_api)

  return input_api.canned_checks.GetUnitTestsRecursively(
      input_api,
      output_api,
      input_api.PresubmitLocalPath(),
      whitelist=['.*_test\\.py$'],
      blacklist=blacklist)


def CommonChecks(input_api, output_api):
  """Presubmit checks run on both upload and commit.
  """
  checks = []
  checks.extend(_GetPythonUnitTests(input_api, output_api))
  return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks on CL upload."""
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks on commit."""
  return CommonChecks(input_api, output_api)

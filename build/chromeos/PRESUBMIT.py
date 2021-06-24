# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for build/chromeos/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""


USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
  results = []
  results += input_api.canned_checks.RunPylint(
      input_api, output_api, pylintrc='pylintrc')
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api, output_api, '.', [r'^.+_test\.py$'], run_on_python3=True)
  results += input_api.RunTests(tests)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

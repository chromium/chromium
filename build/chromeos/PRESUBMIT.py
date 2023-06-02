# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for build/chromeos/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""




def CommonChecks(input_api, output_api):
  results = []
  # These tests don't run on Windows and give verbose and cryptic failure
  # messages. Linting the code on a platform where it will not run is also not
  # valuable and gives spurious errors.
  if input_api.sys.platform != 'win32':
    results += input_api.canned_checks.RunPylint(
        input_api, output_api, pylintrc='pylintrc', version='2.6')
    tests = input_api.canned_checks.GetUnitTestsInDirectory(
        input_api, output_api, '.', [r'^.+_test\.py$'])
    results += input_api.RunTests(tests)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

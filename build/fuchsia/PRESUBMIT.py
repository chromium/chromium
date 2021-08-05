# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for Fuchsia.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  build_fuchsia_dir = input_api.PresubmitLocalPath()

  def J(*dirs):
    """Returns a path relative to presubmit directory."""
    return input_api.os_path.join(build_fuchsia_dir, *dirs)

  tests = []
  tests.extend(
      input_api.canned_checks.GetUnitTests(
          input_api,
          output_api,
          unit_tests=[J('boot_data_test.py'),
                      J('fvdl_target_test.py')],
          run_on_python2=False,
          run_on_python3=True))
  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

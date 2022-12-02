# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for Fuchsia.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True

import os


def CommonChecks(input_api, output_api):
  build_fuchsia_dir = input_api.PresubmitLocalPath()

  def J(*dirs):
    """Returns a path relative to presubmit directory."""
    return input_api.os_path.join(build_fuchsia_dir, *dirs)

  tests = []
  unit_tests = [
      J('binary_sizes_test.py'),
      J('binary_size_differ_test.py'),
      J('device_target_test.py'),
      J('gcs_download_test.py'),
      J('update_images_test.py'),
      J('update_product_bundles_test.py'),
      J('update_sdk_test.py'),
  ]

  # TODO(1309977): enable on Windows when fixed.
  if os.name != 'nt':
    unit_tests.extend([J('fvdl_target_test.py')])
  tests.extend(
      input_api.canned_checks.GetUnitTests(input_api,
                                           output_api,
                                           unit_tests=unit_tests,
                                           run_on_python2=False,
                                           run_on_python3=True,
                                           skip_shebang_check=True))
  return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

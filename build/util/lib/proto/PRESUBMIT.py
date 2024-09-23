# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for build.util.lib.proto.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

import os


def CommonChecks(input_api, output_api):
  build_dir = input_api.PresubmitLocalPath()

  tests = [
      input_api.os_path.join(build_dir, f) for f in os.listdir('.')
      if os.path.isfile(f) and f.endswith('tests.py')
  ]
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTests(input_api,
                                           output_api,
                                           unit_tests=tests))


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for accessibility tools."""

PRESUBMIT_VERSION = '2.0.0'


def CheckPythonUnitTests(input_api, output_api):
  return input_api.RunTests(
    input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      '.',
      files_to_check=[r'^.+_(?:unit)?test\.py$'],
    )
  )

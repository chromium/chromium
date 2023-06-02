# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


TEST_PATTERNS = [r'.+_test.py$']


def CheckUnitTests(input_api, output_api):
  # Runs all unit tests under the build/ios folder.
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=TEST_PATTERNS)

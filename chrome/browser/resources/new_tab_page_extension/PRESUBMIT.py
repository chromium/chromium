# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckPackageNtpModulesTest(input_api, output_api):
  if not input_api.AffectedFiles(
    file_filter=lambda f: input_api.os_path.basename(f.LocalPath()).startswith(
      'package_ntp_modules'
    )
  ):
    return []
  return input_api.canned_checks.RunUnitTests(
    input_api,
    output_api,
    [
      input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'package_ntp_modules_test.py'
      )
    ],
  )

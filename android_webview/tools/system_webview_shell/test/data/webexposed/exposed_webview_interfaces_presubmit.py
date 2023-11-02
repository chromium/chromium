# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckNotWebViewExposedInterfaces(input_api, output_api):
  os_path = input_api.os_path
  script_path = os_path.join(os_path.dirname(__file__),
                             'validate_not_webview_exposed.py')
  _, errors = input_api.subprocess.Popen(
      [input_api.python3_executable, script_path],
      stdout=input_api.subprocess.PIPE,
      stderr=input_api.subprocess.PIPE).communicate()

  if not errors:
    return []
  return [output_api.PresubmitError(errors)]

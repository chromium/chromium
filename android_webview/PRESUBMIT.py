# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for //android_webview/

Gates against using Context#bindService API before upload.
"""

USE_PYTHON3 = True


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CheckNo_Context_bindService_Added(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckNo_Context_bindService_Added(input_api, output_api))
  return results


def _CheckNo_Context_bindService_Added(input_api, output_api):
  """Checks that new no files under //android_webview directly use the
  Context#bindService. This is because Android platform disallows calling
  Context#bindService() from within a BroadcastReceiver context.
  """
  errors = []
  run_with_pattern_part_api = input_api.re.compile(
      r'.*bindService.*')

  def _FilterFile(affected_file):
    skipFiles = (input_api.DEFAULT_FILES_TO_SKIP +
                  (r'.*android_webview[\\\/]js_sandbox[\\\/].*',))
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=skipFiles,
        files_to_check=[r'.+\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = run_with_pattern_part_api.search(line)
      if match:
        if "ServiceHelper.bindService" not in line:
          errors.append("%s:%d:%s" % (f.LocalPath(), line_num, line))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
New code in //android_webview should not use \
android.content.Context#bindService. Instead use \
android_webview.common.services.ServiceHelper#bindService.
""", errors))

  return results

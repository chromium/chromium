# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


SKIP_PRESUBMIT_FILES = set([
  # The file is generated from FFMpeg by Emscripten, and doesn't pass Chrome JS
  # style check. In particular, js_checker.VariableNameCheck fails for this.
  'ash/webui/camera_app_ui/resources/js/lib/ffmpeg.js'
])


def _CommonChecks(input_api, output_api):
  results = input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                        check_js=True)
  try:
    import sys
    old_sys_path = sys.path[:]
    cwd = input_api.PresubmitLocalPath()
    sys.path += [input_api.os_path.join(cwd, '..', '..', 'tools')]
    from web_dev_style import presubmit_support
    results += presubmit_support.CheckStyle(
        input_api,
        output_api,
        lambda x: x.LocalPath() not in SKIP_PRESUBMIT_FILES)
  finally:
    sys.path = old_sys_path
  return results

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True

def _CheckChangeOnUploadOrCommit(input_api, output_api):
  results = []
  webui_sources = set(['optimize_webui.py', 'rollup_plugin.js'])
  affected = input_api.AffectedFiles()
  affected_files = [input_api.os_path.basename(f.LocalPath()) for f in affected]
  if webui_sources.intersection(set(affected_files)):
    results += RunOptimizeWebUiTests(input_api, output_api)
  return results


def RunOptimizeWebUiTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()
  sources = ['optimize_webui_test.py']
  tests = [input_api.os_path.join(presubmit_path, s) for s in sources]
  return input_api.canned_checks.RunUnitTests(
      input_api, output_api, tests, run_on_python2=False)


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChangeOnUploadOrCommit(input_api, output_api)


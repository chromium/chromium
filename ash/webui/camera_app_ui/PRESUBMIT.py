# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


BASE_DIRECTORY = 'ash/webui/camera_app_ui/'
STRING_RESOURCE_FILES = [os.path.join(BASE_DIRECTORY, f) for f in [
  'resources.h',
  'resources/js/i18n_string.ts',
  'resources/strings/camera_strings.grd',
]]


def _CommonChecks(input_api, output_api):
  results = input_api.canned_checks.CheckPatchFormatted(input_api, output_api,
                                                        check_js=True)
  affected = input_api.AffectedFiles()
  if any(f for f in affected if f.LocalPath().endswith('.html')):
    results += _CheckHtml(input_api, output_api)
  if any(f for f in affected if f.LocalPath() in STRING_RESOURCE_FILES):
    results += _CheckStringResouce(input_api, output_api)
  if any(f
         for f in affected
         if f.LocalPath().endswith('.css') or f.LocalPath().endswith('.svg')):
    results += _CheckColorTokens(input_api, output_api)
  if any(f for f in affected if f.LocalPath().endswith('metrics.ts')):
    results += _CheckModifyMetrics(input_api, output_api)

  return results


def _CheckHtml(input_api, output_api):
  return input_api.canned_checks.CheckLongLines(
      input_api, output_api, 80, lambda x: x.LocalPath().endswith('.html'))


def _CheckStringResouce(input_api, output_api):
  rv = input_api.subprocess.call([
      input_api.python3_executable, './resources/utils/cca.py',
      'check-strings'
  ])

  if rv:
    return [
        output_api.PresubmitPromptWarning(
            'String resources check failed, ' +
            'please make sure the relevant string files are all modified.')
    ]

  return []


def _CheckColorTokens(input_api, output_api):
  rv = input_api.subprocess.call([
      input_api.python3_executable, './resources/utils/cca.py',
      'check-color-tokens'
  ])

  if rv:
    return [
        output_api.PresubmitPromptWarning(
            'Color token check failed, ' +
            'please only use new dynamic color tokens in new CSS rules.')
    ]

  return []


def _CheckModifyMetrics(input_api, output_api):
  if input_api.no_diffs or input_api.change.METRICS_DOCUMENTATION_UPDATED:
    return []
  return [
    output_api.PresubmitPromptWarning(
        'Metrics are modified but `METRICS_DOCUMENTATION_UPDATED=true` is ' +
        'not found in the commit messages.\n' +
        'The CL author should confirm CCA metrics are still synced in ' +
        'PDD (go/cca-metrics-pdd) and Schema (go/cca-metrics-schema).\n' +
        'Once done, the CL author should explicitly claim it by including ' +
        '`METRICS_DOCUMENTATION_UPDATED=true` in the commit messages.'
    )
  ]

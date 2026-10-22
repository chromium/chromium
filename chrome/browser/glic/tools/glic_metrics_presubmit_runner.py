# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared presubmit runner for Glic metrics checks."""

import json


def RunGlicMetricsCheck(input_api, output_api, checks):
  """Runs tools/check_glic_metrics.py for the given checks ('cpp', 'xml')."""
  import check_glic_metrics

  _HISTOGRAMS_PATH = 'tools/metrics/histograms/metadata/glic/histograms.xml'

  affected_files = []
  has_relevant_files = False

  for f in input_api.change.AffectedFiles(include_deletes=False):
    path = f.LocalPath().replace('\\', '/')
    is_histograms = path == _HISTOGRAMS_PATH
    is_cpp = check_glic_metrics.is_glic_cpp_source_file(path)

    if is_histograms or (is_cpp and 'cpp' in checks):
      has_relevant_files = True
      file_info = {
        'path': path,
        'new_contents': list(f.NewContents()),
        'changed_lines': [num for num, _ in f.ChangedContents()],
      }
      if is_histograms:
        file_info['old_contents'] = list(f.OldContents() or [])
      affected_files.append(file_info)

  if not has_relevant_files:
    return []

  payload = {
    'checks': checks,
    'affected_files': affected_files,
  }
  script_path = input_api.os_path.join(
    input_api.change.RepositoryRoot(),
    'chrome',
    'browser',
    'glic',
    'tools',
    'check_glic_metrics.py',
  )
  proc = input_api.subprocess.Popen(
    [input_api.python3_executable, script_path, '--presubmit-json'],
    cwd=input_api.change.RepositoryRoot(),
    stdin=input_api.subprocess.PIPE,
    stdout=input_api.subprocess.PIPE,
    stderr=input_api.subprocess.PIPE,
    text=True,
  )
  stdout, stderr = proc.communicate(json.dumps(payload))
  try:
    errors = json.loads(stdout)
  except ValueError:
    errors = None
  if not isinstance(errors, list):
    return [
      output_api.PresubmitError(
        f'check_glic_metrics.py failed (exit code {proc.returncode}):\n'
        f'{stderr or stdout}'
      )
    ]
  return [output_api.PresubmitError(err) for err in errors]

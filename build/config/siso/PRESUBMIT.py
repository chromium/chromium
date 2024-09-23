# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckSisoConfigFormat(input_api, output_api):
  """Check if build/config/siso/*.star files are formatted correctly."""
  repo_root = input_api.change.RepositoryRoot()
  log_level = 'debug' if input_api.verbose else 'warning'
  commands = []
  for f in input_api.AffectedFiles():
    filepath = f.AbsoluteLocalPath()
    if not filepath.endswith('.star'):
      continue
    if not input_api.os_path.isfile(filepath):
      continue
    name = 'Validate ' + filepath
    cmd = ['lucicfg', 'fmt', '-dry-run', '-log-level', log_level, filepath]
    commands.append(input_api.Command(name, cmd, {}, output_api.PresubmitError))
  return input_api.RunTests(commands)

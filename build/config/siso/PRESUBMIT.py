# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'


def CheckTryjobFooters(input_api, output_api):
  """Check if footers include Cq-Include-Trybots to trigger Siso tryjobs."""
  footerTryjobs = input_api.change.GitFootersFromDescription().get(
      'Cq-Include-Trybots', [])
  if footerTryjobs:
    return []

  message = (
      "Missing 'Cq-Include-Trybots:' field required for Siso config changes"
      "\nPlease add the following fields to run Siso tryjobs.\n\n"
      "Cq-Include-Trybots: luci.chromium.try:linux_chromium_asan_siso_rel_ng\n"
      "Cq-Include-Trybots: luci.chromium.try:linux_chromium_tsan_siso_rel_ng\n"
  )
  return [output_api.PresubmitPromptWarning(message)]


def CheckSisoConfigFormat(input_api, output_api):
  """Check if build/config/siso/*.star files are formatted correctly."""
  repo_root = input_api.change.RepositoryRoot()
  log_level = 'debug' if input_api.verbose else 'warning'
  commands = []
  for f in input_api.AffectedFiles():
    filepath = f.AbsoluteLocalPath()
    if not filepath.endswith('.star'):
      continue
    name = 'Validate ' + filepath
    cmd = ['lucicfg', 'fmt', '-dry-run', '-log-level', log_level, filepath]
    commands.append(input_api.Command(name, cmd, {}, output_api.PresubmitError))
  return input_api.RunTests(commands)

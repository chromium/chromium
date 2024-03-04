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

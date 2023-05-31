# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for Scanning.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys

# Use existing SemanticCssChecker to advise use of cros approved colors
# and semantic variables which support dark-mode display.
# See: ui/chromeos/styles/semantic_css_checker.py
def _CheckSemanticCssColors(input_api, output_api):
  original_sys_path = sys.path
  join = input_api.os_path.join
  src_root = input_api.change.RepositoryRoot()
  try:
    # Change the system path to SemanticCssChecker's directory to be
    # able to import it.
    sys.path.append(join(src_root, 'ui', 'chromeos', 'styles'))
    from semantic_css_checker import SemanticCssChecker
  finally:
    sys.path = original_sys_path

  return SemanticCssChecker.RunChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckSemanticCssColors(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)

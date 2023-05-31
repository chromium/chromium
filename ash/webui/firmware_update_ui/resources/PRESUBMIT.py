# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for Firmware Update

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import sys
from importlib.util import module_from_spec, spec_from_file_location


# Use existing SemanticCssChecker to advise use of cros approved colors
# and semantic variables which support dark-mode display.
# See: ui/chromeos/styles/semantic_css_checker.py
def _CheckSemanticCssColors(input_api, output_api):
    join = input_api.os_path.join
    src_root = input_api.change.RepositoryRoot()
    # Build OS independent path to semantic checker.
    module_path = join(src_root, 'ui', 'chromeos', 'styles',
                       'semantic_css_checker.py')
    spec = spec_from_file_location('semantic_css_checker', module_path)
    checker = module_from_spec(spec)
    # Load checker so it can be used.
    spec.loader.exec_module(checker)
    return checker.SemanticCssChecker.RunChecks(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
    return _CheckSemanticCssColors(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CheckSemanticCssColors(input_api, output_api)

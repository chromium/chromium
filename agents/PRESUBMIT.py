# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for //agents.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

# Extra paths that should be added to PYTHONPATH when running pylint, i.e.
# dependencies on other Chromium Python code.
PYLINT_EXTRA_PATHS_COMPONENTS = [
    ('build', 'util'),
]


def _GetChromiumSrcPath(input_api):
    """Returns the path to the Chromium src directory."""
    return input_api.os_path.realpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), '..'))


def CheckPylint(input_api, output_api):
    """Runs pylint on all directory content and subdirectories."""
    chromium_src_path = _GetChromiumSrcPath(input_api)
    pylint_extra_paths = [
        input_api.os_path.join(chromium_src_path, *component)
        for component in PYLINT_EXTRA_PATHS_COMPONENTS
    ]
    return input_api.RunTests(
        input_api.canned_checks.GetPylint(
            input_api,
            output_api,
            extra_paths_list=pylint_extra_paths,
            version='3.2',
            disabled_warnings=[
                # Often produces non-actionable warnings.
                'duplicate-code',
            ],
        ))

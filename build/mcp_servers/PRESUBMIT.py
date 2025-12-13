# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //build/mcp_servers.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def _GetChromiumSrcPath(input_api):
    """Returns the path to the Chromium src directory."""
    return input_api.os_path.realpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..'))


def CheckUnittests(input_api, output_api):
    """Runs all unittests in the directory and subdirectories."""
    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api,
        output_api,
        input_api.PresubmitLocalPath(),
        [r'^.+_unittest\.py$'],
    )


def CheckPylint(input_api, output_api):
    """Runs pylint on all directory content and subdirectories."""
    chromium_src_path = _GetChromiumSrcPath(input_api)
    extra_path_components = [
        ('build', ),
    ]
    extra_paths = [
        input_api.os_path.join(chromium_src_path, *component)
        for component in extra_path_components
    ]
    pylint_checks = input_api.canned_checks.GetPylint(
        input_api,
        output_api,
        extra_paths_list=extra_paths,
        pylintrc='pylintrc',
        version='3.2')
    return input_api.RunTests(pylint_checks)

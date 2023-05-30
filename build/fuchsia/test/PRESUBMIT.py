# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for build/fuchsia/test.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


_EXTRA_PATHS_COMPONENTS = [('testing', )]

# pylint: disable=invalid-name,missing-function-docstring
def CommonChecks(input_api, output_api):
    # Neither running nor linting Fuchsia tests is supported on Windows.
    if input_api.is_windows:
        return []

    tests = []

    chromium_src_path = input_api.os_path.realpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..',
                               '..'))
    pylint_extra_paths = [
        input_api.os_path.join(chromium_src_path, *component)
        for component in _EXTRA_PATHS_COMPONENTS
    ]
    tests.extend(
        input_api.canned_checks.GetPylint(input_api,
                                          output_api,
                                          extra_paths_list=pylint_extra_paths,
                                          pylintrc='pylintrc',
                                          version='2.7'))

    # coveragetest.py is responsible for running unit tests in this directory
    tests.append(
        input_api.Command(
            name='coveragetest',
            cmd=[input_api.python3_executable, 'coveragetest.py'],
            kwargs={},
            message=output_api.PresubmitError))
    return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)

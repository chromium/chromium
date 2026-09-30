# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for build/fuchsia/test.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
    # Running Fuchsia tests is not supported on Windows.
    if input_api.is_windows:
        return []

    tests = []

    # coveragetest.py is responsible for running unit tests in this directory
    tests.append(
        input_api.Command(
            name='coveragetest',
            cmd=[input_api.python3_executable, 'coveragetest.py'],
            kwargs={},
            message=output_api.PresubmitError,
        )
    )
    return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)

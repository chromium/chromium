# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for build/fuchsia/test.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


# pylint: disable=invalid-name,missing-function-docstring
def CommonChecks(input_api, output_api):
    tests = []
    tests.extend(
        input_api.canned_checks.GetPylint(input_api,
                                          output_api,
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

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
    presubmit_dir = input_api.PresubmitLocalPath()

    def J(*dirs):
        """Returns a path relative to presubmit directory."""

        return input_api.os_path.join(presubmit_dir, *dirs)

    tests = []
    unit_tests = [
        J('publish_package_unittests.py'),
    ]

    tests.extend(
        input_api.canned_checks.GetPylint(input_api,
                                          output_api,
                                          pylintrc='pylintrc',
                                          version='2.7'))
    tests.extend(
        input_api.canned_checks.GetUnitTests(input_api,
                                             output_api,
                                             unit_tests=unit_tests,
                                             run_on_python2=False,
                                             run_on_python3=True))
    return input_api.RunTests(tests)


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)

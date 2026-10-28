# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CommonChecks(input_api, output_api):
    test_dir = input_api.os_path.join(input_api.PresubmitLocalPath(), 'test')

    env = dict(input_api.environ)
    env['PYTHONDONTWRITEBYTECODE'] = '1'

    return input_api.RunTests(
        input_api.canned_checks.GetUnitTests(
            input_api,
            output_api,
            unit_tests=[
                input_api.os_path.join(test_dir, 'parse_test.py'),
                input_api.os_path.join(test_dir, 'integration_tests.py'),
            ],
            env=env,
        ))


def CheckChangeOnUpload(input_api, output_api):
    return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return CommonChecks(input_api, output_api)

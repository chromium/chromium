# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //build/skia_gold_common/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

USE_PYTHON3 = True


def CommonChecks(input_api, output_api):
  output = []
  build_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '..')
  skia_gold_env = dict(input_api.environ)
  skia_gold_env.update({
      'PYTHONPATH': build_path,
      'PYTHONDONTWRITEBYTECODE': '1',
  })
  output.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.PresubmitLocalPath(), [r'^.+_unittest\.py$'],
          env=skia_gold_env))
  output.extend(input_api.canned_checks.RunPylint(input_api, output_api))
  return output


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)

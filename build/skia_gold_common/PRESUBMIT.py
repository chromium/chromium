# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //build/skia_gold_common/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


PRESUBMIT_VERSION = '2.0.0'


def _GetSkiaGoldEnv(input_api):
  """Gets the common environment for running Skia Gold tests."""
  build_path = input_api.os_path.join(input_api.PresubmitLocalPath(), '..')
  skia_gold_env = dict(input_api.environ)
  skia_gold_env.update({
      'PYTHONPATH': build_path,
      'PYTHONDONTWRITEBYTECODE': '1',
  })
  return skia_gold_env


def CheckSkiaGoldCommonUnittests(input_api, output_api):
  """Runs the unittests for the build/skia_gold_common/ directory."""
  return input_api.canned_checks.RunUnitTestsInDirectory(
      input_api,
      output_api,
      input_api.PresubmitLocalPath(), [r'^.+_unittest\.py$'],
      env=_GetSkiaGoldEnv(input_api))


def CheckPylint(input_api, output_api):
  """Runs pylint on all directory content and subdirectories."""
  return input_api.canned_checks.RunPylint(input_api, output_api, version='2.7')

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for app_list.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

INCLUDE_CPP_FILES_ONLY = (
  r'.*\.(cc|h)$',
)

def CheckChangeLintsClean(input_api, output_api):
  """Makes sure that the code is cpplint clean."""
  files_to_skip = input_api.DEFAULT_FILES_TO_SKIP
  sources = lambda x: input_api.FilterSourceFile(
    x, files_to_check=INCLUDE_CPP_FILES_ONLY, files_to_skip=files_to_skip)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, sources, lint_filters=[], verbose_level=1)

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  return results

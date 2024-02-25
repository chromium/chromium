# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def _CheckNoJsChanges(input_api, output_api):
  """Enforce that JavaScript files are not changed.

  During the migration to TypeScript, the closure compiler has been disabled.
  That means any changes in JavaScript will not be run through a type compiler,
  which is not good. Convert to TypeScript before making changes.
  """
  results = []
  for f in input_api.AffectedFiles(False):  # ignores deleted files.
    path = f.LocalPath()
    filename = os.path.basename(path)
    if (
        path.endswith("js")
        and not path.endswith("test.js")
        and path.find("_test") == -1
        and path.find("test_") == -1
        and path.find("common/testing/") == -1
        and not filename[0] == "."
    ):
      results.append(
          output_api.PresubmitError(
              "Illegal file modification: %s\n\nModifying JavaScript files in"
              " the accessibility/ subdirectory is not allowed during the"
              " TypeScript migration. Please migrate this file to TypeScript"
              " before making any changes."
              % f.LocalPath()
          )
      )
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckNoJsChanges(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return []

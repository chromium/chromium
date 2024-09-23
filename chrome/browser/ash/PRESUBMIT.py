# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True

def _CheckDepsFileProhibitingChromeExists(input_api, output_api):
  """Enforce that each top-level directory in //chrome/browser/ash has DEPS file
  which prohibits //chrome.

  This ensures that any new //chrome dependencies added in this directory are
  reviewed by a //chrome OWNER. There is an active effort to refactor
  //chrome/browser/ash to break these dependencies; see b/332804822.
  """
  _CHROME_BROWSER_ASH = input_api.os_path.join('chrome', 'browser', 'ash')

  missing_deps_files = set()
  deps_files_not_prohibiting_chrome = set()

  for f in input_api.AffectedFiles(False):
    # Path relative to chrome/browser/ash.
    # Example: For 'chrome/browser/ash/foo/bar/baz.h' => 'foo/bar/baz.h'.
    relative_path = input_api.os_path.relpath(f.LocalPath(),
                                              _CHROME_BROWSER_ASH)

    # Split path for this relative path.
    # Example: For 'foo/bar/baz.h' => ['foo' 'bar/baz.h'].
    splitPath = relative_path.split(input_api.os_path.sep, 1)

    # If the split path contains 1 or fewer elements, it is not in a
    # subdirectory (e.g., len == 1 would be a file directly in
    # //chrome/browser/ash).
    if len(splitPath) <= 1:
      continue

    # DEPS relative to chrome/browser/ash.
    relative_deps_files_path = input_api.os_path.join(splitPath[0], 'DEPS')
    # Absolute DEPS path.
    deps_file_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            relative_deps_files_path)
    # DEPS relative to repo root.
    local_deps_file_path = input_api.os_path.join(_CHROME_BROWSER_ASH,
                                                  relative_deps_files_path)

    if not input_api.os_path.exists(deps_file_path):
      missing_deps_files.add(local_deps_file_path)
      continue

    # If the affected file is not a DEPS file, move onto the next file to check.
    if f.LocalPath() != local_deps_file_path:
      continue

    # If the affected file *is* a DEPS file, confirm that it has a "-chrome"
    # rule, prohibiting new //chrome dependencies.
    prohibit_chrome_pattern = input_api.re.compile(r'\"\-chrome\"')
    if not prohibit_chrome_pattern.search(input_api.ReadFile(deps_file_path)):
      deps_files_not_prohibiting_chrome.add(local_deps_file_path)

  results = []
  if missing_deps_files:
    dir_text = ''
    for dir in missing_deps_files:
      dir_text += dir + ', '
    dir_text = dir_text[:-2]
    results.append(output_api.PresubmitError(
        "Subdirectories in //chrome/browser/ash require a DEPS file. Please "
        "create DEPS files: [%s]. See b/332805865 and "
        "//tools/chromeos/gen_deps.sh for details." % (dir_text)
        ))

  if deps_files_not_prohibiting_chrome:
    deps_text = ''
    for deps_file in deps_files_not_prohibiting_chrome:
      deps_text += deps_file + ', '
    deps_text = deps_text[:-2]
    results.append(output_api.PresubmitError(
        "DEPS files in subdirectories of in //chrome/browser/ash must prohibit "
        "new //chrome dependencies. Please ensure a \"-chrome\" rule is added "
        "in [%s]. See b/332805865 and //tools/chromeos/gen_deps.sh for "
        " details." % (deps_text)
        ))

  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CheckDepsFileProhibitingChromeExists(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckDepsFileProhibitingChromeExists(input_api, output_api)

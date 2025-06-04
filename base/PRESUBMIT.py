# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/base.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

def CheckChangeLintsClean(input_api, output_api):
  """Makes sure that the code is cpplint clean."""
  # lint_filters=[] stops the OFF_BY_DEFAULT_LINT_FILTERS from being disabled,
  # finding many more issues. verbose_level=1 finds a small number of additional
  # issues.
  # The only valid extensions for cpplint are .cc, .h, .cpp, .cu, and .ch.
  # Only process those extensions which are used in Chromium, in directories
  # that currently lint clean.
  CLEAN_CPP_FILES_ONLY = (r'base/win/.*\.(cc|h)$', )
  source_file_filter = lambda x: input_api.FilterSourceFile(
      x,
      files_to_check=CLEAN_CPP_FILES_ONLY,
      files_to_skip=input_api.DEFAULT_FILES_TO_SKIP)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_file_filter=source_file_filter,
      lint_filters=[], verbose_level=1)


def _CheckNoInterfacesInBase(input_api, output_api):
  """Checks to make sure no files in libbase.a have |@interface|."""
  pattern = input_api.re.compile(r'^\s*@interface', input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('base/') and
        not "/ios/" in f.LocalPath() and
        not "/test/" in f.LocalPath() and
        not f.LocalPath().endswith('.java') and
        not f.LocalPath().endswith('_unittest.mm') and
        not f.LocalPath().endswith('_spi.h')):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitError(
        'Objective-C interfaces or categories are forbidden in libbase. ' +
        'See http://groups.google.com/a/chromium.org/group/chromium-dev/' +
        'browse_thread/thread/efb28c10435987fd',
        files) ]
  return []


def _FindLocations(input_api, search_regexes, files_to_check, files_to_skip):
  """Returns locations matching one of the search_regexes."""
  def FilterFile(affected_file):
    return input_api.FilterSourceFile(
      affected_file,
      files_to_check=files_to_check,
      files_to_skip=files_to_skip)

  no_presubmit = r"// no-presubmit-check"
  locations = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    for line_num, line in f.ChangedContents():
      for search_regex in search_regexes:
        if (input_api.re.search(search_regex, line) and
            not input_api.re.search(no_presubmit, line)):
          locations.append("    %s:%d" % (f.LocalPath(), line_num))
          break
  return locations


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoInterfacesInBase(input_api, output_api))
  results.extend(CheckChangeLintsClean(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results

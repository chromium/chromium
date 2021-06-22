# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/base.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


USE_PYTHON3 = True


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
        not f.LocalPath().endswith('mac/sdk_forward_declarations.h')):
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


def _CheckNoTraceEventInclude(input_api, output_api):
  """Verify that //base includes base_tracing.h instead of trace event headers.

  Checks that files outside trace event implementation include the
  base_tracing.h header instead of specific trace event implementation headers
  to maintain compatibility with the gn flag "enable_base_tracing = false".
  """
  discouraged_includes = [
    r'^#include "base/trace_event/(?!base_tracing\.h|base_tracing_forward\.h)',
    r'^#include "third_party/perfetto/include/',
  ]

  files_to_check = [
    r".*\.(h|cc|mm)$",
  ]
  files_to_skip = [
    r".*[\\/]test[\\/].*",
    r".*[\\/]trace_event[\\/].*",
    r".*[\\/]tracing[\\/].*",
  ]

  locations = _FindLocations(input_api, discouraged_includes, files_to_check,
                             files_to_skip)
  if locations:
    return [ output_api.PresubmitError(
        'Base code should include "base/trace_event/base_tracing.h" instead\n' +
        'of trace_event implementation headers. If you need to include an\n' +
        'implementation header, verify that "gn check" and base_unittests\n' +
        'still pass with gn arg "enable_base_tracing = false" and add\n' +
        '"// no-presubmit-check" after the include. \n' +
        '\n'.join(locations)) ]
  return []


def _WarnPbzeroIncludes(input_api, output_api):
  """Warn to check enable_base_tracing=false when including a pbzero header.

  Emits a warning when including a perfetto pbzero header, encouraging the
  user to verify that //base still builds with enable_base_tracing=false.
  """
  warn_includes = [
    r'^#include "third_party/perfetto/protos/',
    r'^#include "base/tracing/protos/',
  ]

  files_to_check = [
    r".*\.(h|cc|mm)$",
  ]
  files_to_skip = [
    r".*[\\/]test[\\/].*",
    r".*[\\/]trace_event[\\/].*",
    r".*[\\/]tracing[\\/].*",
  ]

  locations = _FindLocations(input_api, warn_includes, files_to_check,
                             files_to_skip)
  if locations:
    return [ output_api.PresubmitPromptWarning(
        'Please verify that "gn check" and base_unittests still pass with\n' +
        'gn arg "enable_base_tracing = false" when adding typed trace\n' +
        'events to //base. You can use "#if BUILDFLAG(ENABLE_BASE_TRACING)"\n' +
        'to exclude pbzero headers and anything not supported by\n' +
        '//base/trace_event/trace_event_stub.h.\n' +
        '\n'.join(locations)) ]
  return []


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckNoInterfacesInBase(input_api, output_api))
  results.extend(_CheckNoTraceEventInclude(input_api, output_api))
  results.extend(_WarnPbzeroIncludes(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results

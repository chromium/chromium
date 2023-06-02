# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for android_webview/support_library/

Runs various style checks before upload.
"""

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckAnnotatedInvocationHandlers(input_api, output_api))
  results.extend(_CheckFeatureDevSuffix(input_api, output_api))
  return results

def _CheckAnnotatedInvocationHandlers(input_api, output_api):
  """Checks that all references to InvocationHandlers are annotated with a
  comment describing the class the InvocationHandler represents. This does not
  check .../support_lib_boundary/util/, because this has legitimate reasons to
  refer to InvocationHandlers without them standing for a specific type.
  """

  invocation_handler_str = r'\bInvocationHandler\b'
  annotation_str = r'/\* \w+ \*/\s+'
  invocation_handler_import_pattern = input_api.re.compile(
      r'^import.*' + invocation_handler_str + ';$')
  possibly_annotated_handler_pattern = input_api.re.compile(
      r'(' + annotation_str + r')?(' + invocation_handler_str + r')')

  errors = []

  sources = lambda affected_file: input_api.FilterSourceFile(
      affected_file,
      files_to_skip=(input_api.DEFAULT_FILES_TO_SKIP +
                  (r'.*support_lib_boundary[\\\/]util[\\\/].*',)),
      files_to_check=(r'.*\.java$',))

  for f in input_api.AffectedSourceFiles(sources):
    for line_num, line in f.ChangedContents():
      if not invocation_handler_import_pattern.search(line):
        for match in possibly_annotated_handler_pattern.findall(line):
          annotation = match[0]
          if not annotation:
            # Note: we intentionally double-count lines which have multiple
            # mistakes, since we require each mention of 'InvocationHandler' to
            # be annotated.
            errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
All references to InvocationHandlers should be annotated with the type they
represent using a comment, e.g.:

/* RetType */ InvocationHandler method(/* ParamType */ InvocationHandler param);
""",
        errors))

  return results

def _CheckFeatureDevSuffix(input_api, output_api):
  """Checks that Features.DEV_SUFFIX is not used in boundary_interfaces. The
  right place to use it is SupportLibWebViewChromiumFactory.
  """

  pattern = input_api.re.compile(r'\bDEV_SUFFIX\b')

  problems = []
  filt = lambda f: 'boundary_interfaces' in f.LocalPath()
  for f in input_api.AffectedFiles(file_filter=filt):
    for line_num, line in f.ChangedContents():
      m = pattern.search(line)
      if m:
        problems.append('  %s:%d\n    %s\n' % (f.LocalPath(), line_num, line))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning(
    'DEV_SUFFIX should not be used in boundary_interfaces.\n' + '\n'
    .join(problems))]

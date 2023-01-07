# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for android_webview/javatests/

Runs various style checks before upload.
"""

USE_PYTHON3 = True


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckAwJUnitTestRunner(input_api, output_api))
  results.extend(_CheckNoSkipCommandLineAnnotation(input_api, output_api))
  results.extend(_CheckNoSandboxedRendererSwitch(input_api, output_api))
  results.extend(_CheckNoDomUtils(input_api, output_api))
  return results


def _CheckAwJUnitTestRunner(input_api, output_api):
  """Checks that new tests use the AwJUnit4ClassRunner instead of some other
  test runner. This is because WebView has special logic in the
  AwJUnit4ClassRunner.
  """

  run_with_pattern = input_api.re.compile(
      r'^@RunWith\((.*)\)$')
  correct_runner = 'AwJUnit4ClassRunner.class'

  errors = []

  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = run_with_pattern.search(line)
      if match and match.group(1) != correct_runner:
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
android_webview/javatests/ should use the AwJUnit4ClassRunner test runner, not
any other test runner (e.g., BaseJUnit4ClassRunner).
""", errors))

  return results


def _CheckNoSkipCommandLineAnnotation(input_api, output_api):
  """Checks that tests do not add @SkipCommandLineParameterization annotation.
  This was previously used to run the test in single-process-mode only (or,
  multi-process-mode only if used with
  @CommandLineFlags.Add(AwSwitches.WEBVIEW_SANDBOXED_RENDERER)). This is
  obsolete because we have dedicated annotations (@OnlyRunInSingleProcessMode
  and @OnlyRunInMultiProcessMode).
  """

  skip_command_line_annotation = input_api.re.compile(
      r'^\s*@SkipCommandLineParameterization.*$')

  errors = []

  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = skip_command_line_annotation.search(line)
      if match:
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
android_webview/javatests/ should not use @SkipCommandLineParameterization to
run in either multi-process or single-process only. Instead, use @OnlyRunIn.
""", errors))

  return results


def _CheckNoSandboxedRendererSwitch(input_api, output_api):
  """Checks that tests do not add the AwSwitches.WEBVIEW_SANDBOXED_RENDERER
  command line flag. Tests should instead use @OnlyRunIn(MULTI_PROCESS).
  """

  # This will not catch multi-line annotations (which are valid if adding
  # multiple switches), but is better than nothing (and avoids false positives).
  sandboxed_renderer_pattern = input_api.re.compile(
      r'^\s*@CommandLineFlags\.Add\(.*'
      r'\bAwSwitches\.WEBVIEW_SANDBOXED_RENDERER\b.*\)$')

  errors = []

  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      match = sandboxed_renderer_pattern.search(line)
      if match:
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []

  if errors:
    results.append(output_api.PresubmitPromptWarning("""
android_webview/javatests/ should not use AwSwitches.WEBVIEW_SANDBOXED_RENDERER
to run in multi-process only. Instead, use @OnlyRunIn(MULTI_PROCESS).
""", errors))

  return results


def _CheckNoDomUtils(input_api, output_api):
  """Checks that tests prefer JSUtils.clickNodeWithUserGesture() over
  DOMUtils.clickNode().
  """

  dom_utils_pattern = input_api.re.compile(r'DOMUtils\.clickNode\(')
  errors = []
  def _FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_skip=input_api.DEFAULT_FILES_TO_SKIP,
        files_to_check=[r'.*\.java$'])

  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_num, line in f.ChangedContents():
      m = dom_utils_pattern.search(line)
      if m:
        errors.append("%s:%d" % (f.LocalPath(), line_num))

  results = []
  if errors:
    results.append(output_api.PresubmitPromptWarning("""
DOMUtils.clickNode() has been observed to cause flakiness in WebView tests.
Prefer using JSUtils.clickNodeWithUserGesture() as a more reliable replacement
where possible. This is a "soft" warning, so you can bypass this if
DOMUtils.clickNode() is the only way.
""", errors))

  return results



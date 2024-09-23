# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser code."""


import re

# Checks whether an autofill-related browsertest fixture class inherits from
# either InProcessBrowserTest or AndroidBrowserTest without having a member of
# type `autofill::test::AutofillBrowserTestEnvironment`. In that case, the
# functions registers a presubmit warning.
def _CheckNoAutofillBrowserTestsWithoutAutofillBrowserTestEnvironment(
        input_api, output_api):
  autofill_files_pattern = re.compile(
      r'(autofill|password_manager).*\.(mm|cc|h)')
  concerned_files = [(f, input_api.ReadFile(f))
                     for f in input_api.AffectedFiles(include_deletes=False)
                     if autofill_files_pattern.search(f.LocalPath())]

  warning_files = []
  class_name = r'^( *)(class|struct)\s+\w+\s*:\s*'
  target_base = r'[^\{]*\bpublic\s+(InProcess|Android)BrowserTest[^\{]*\{'
  class_declaration_pattern = re.compile(
      class_name + target_base, re.MULTILINE)
  for autofill_file, file_content in concerned_files:
    for class_match in re.finditer(class_declaration_pattern, file_content):
      indentation = class_match.group(1)
      class_end_pattern = re.compile(
          r'^' + indentation + r'\};$', re.MULTILINE)
      class_end = class_end_pattern.search(file_content[class_match.start():])

      corresponding_subclass = (
          '' if class_end is None else
          file_content[
              class_match.start():
              class_match.start() + class_end.end()])

      required_member_pattern = re.compile(
          r'^' + indentation +
          r'  (::)?(autofill::)?test::AutofillBrowserTestEnvironment\s+\w+_;',
          re.MULTILINE)
      if not required_member_pattern.search(corresponding_subclass):
        warning_files.append(autofill_file)

  return [output_api.PresubmitPromptWarning(
      'Consider adding a member autofill::test::AutofillBrowserTestEnvironment '
      'to the test fixtures that derive from InProcessBrowserTest or '
      'AndroidBrowserTest in order to disable kAutofillServerCommunication in '
      'browser tests.',
      warning_files)] if len(warning_files) else []

def _RunHistogramChecks(input_api, output_api, histogram_name):
  try:
    # Setup sys.path so that we can call histograms code.
    import sys
    original_sys_path = sys.path
    sys.path = sys.path + [input_api.os_path.join(
        input_api.change.RepositoryRoot(),
        'tools', 'metrics', 'histograms')]

    results = []

    import presubmit_bad_message_reasons
    results.extend(presubmit_bad_message_reasons.PrecheckBadMessage(input_api,
        output_api, histogram_name))

    return results
  except:
    return [output_api.PresubmitError('Could not verify histogram!')]
  finally:
    sys.path = original_sys_path


def _CheckUnwantedDependencies(input_api, output_api):
  problems = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('DEPS'):
      continue

    for line_num, line in f.ChangedContents():
      if not line.strip().startswith('#'):
        m = re.search(r".*\/blink\/public\/web.*", line)
        if m:
          problems.append(m.group(0))

  if not problems:
    return []
  return [output_api.PresubmitPromptWarning(
      'chrome/browser cannot depend on blink/public/web interfaces. ' +
      'Use blink/public/common instead.',
      items=problems)]


def _CheckNoInteractiveUiTestLibInNonInteractiveUiTest(input_api, output_api):
  """Makes sure that ui_controls related API are used only in
  interactive_in_tests.
  """
  problems = []
  # There are interactive tests whose name ends with `_browsertest.cc`
  # or `_browser_test.cc`.
  files_to_skip = ((r'.*interactive_.*test\.cc',) +
                   input_api.DEFAULT_FILES_TO_SKIP)
  def FileFilter(affected_file):
    """Check non interactive_uitests only."""
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=(
            r'.*browsertest\.cc',
            r'.*unittest\.cc'),
        files_to_skip=files_to_skip)

  ui_controls_includes =(
    input_api.re.compile(
        r'#include.*/(ui_controls.*h|interactive_test_utils.h)"'))

  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=FileFilter):
    for line_num, line in f.ChangedContents():
      m = re.search(ui_controls_includes, line)
      if m:
        problems.append('  %s:%d:%s' % (f.LocalPath(), line_num, m.group(0)))

  if not problems:
    return []

  WARNING_MSG ="""
  ui_controls API can be used only in interactive_ui_tests.
  If the test is in the interactive_ui_tests, please consider renaming
  to xxx_interactive_uitest.cc"""
  return [output_api.PresubmitPromptWarning(WARNING_MSG, items=problems)]


def _CheckForUselessExterns(input_api, output_api):
  """Makes sure developers don't copy "extern const char kFoo[]" from
  foo.h to foo.cc.
  """
  problems = []
  BAD_PATTERN = input_api.re.compile(r'^extern const')

  def FileFilter(affected_file):
    """Check only a particular list of files"""
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=[r'chrome[/\\]browser[/\\]flag_descriptions\.cc']);

  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=FileFilter):
    for _, line in f.ChangedContents():
      if BAD_PATTERN.search(line):
        problems.append(f)

  if not problems:
    return []

  WARNING_MSG ="""Do not write "extern const char" in these .cc files:"""
  return [output_api.PresubmitPromptWarning(WARNING_MSG, items=problems)]


def _CheckBuildFilesForIndirectAshSources(input_api, output_api):
  """Warn when indirect paths are added to an ash target's "sources".

  Indirect paths are paths containing a slash, e.g. "foo/bar.h" or "../foo.cc".
  """

  MSG = ("It appears that sources were added to the above BUILD.gn file but "
         "their paths contain a slash, indicating that the files are from a "
         "different directory (e.g. a subdirectory). As a general rule, Ash "
         "sources should live in the same directory as the BUILD.gn file "
         "listing them. There may be cases where this is not feasible or "
         "doesn't make sense, hence this is only a warning. If in doubt, "
         "please contact ash-chrome-refactor-wg@google.com.")

  os_path = input_api.os_path

  # Any BUILD.gn in or under one of these directories will be checked.
  monitored_dirs = [
      os_path.join("chrome", "browser", "ash"),
      os_path.join("chrome", "browser", "chromeos"),
      os_path.join("chrome", "browser", "ui", "ash"),
      os_path.join("chrome", "browser", "ui", "chromeos"),
      os_path.join("chrome", "browser", "ui", "webui", "ash"),
  ]
  def should_check_path(affected_path):
    if os_path.basename(affected_path) != 'BUILD.gn':
      return False
    ad = os_path.dirname(affected_path)
    for md in monitored_dirs:
      if os_path.commonpath([ad, md]) == md:
        return True
    return False

  # Simplifying assumption: 'sources' keyword always appears at the beginning of
  # a line (optionally preceded by whitespace).
  sep = r'(?m:\s*#.*$)*\s*'  # whitespace and/or comments, possibly empty
  sources_re = re.compile(
      fr'(?m:^\s*sources{sep}\+?={sep}\[((?:{sep}"[^"]*"{sep},?{sep})*)\])')
  source_re = re.compile(fr'{sep}"([^"]*)"')

  def find_indirect_sources(contents):
    result = []
    for sources_m in sources_re.finditer(contents):
      for source_m in source_re.finditer(sources_m.group(1)):
        source = source_m.group(1)
        if '/' in source:
          result.append(source)
    return result

  results = []
  for f in input_api.AffectedTestableFiles():
    if not should_check_path(f.LocalPath()):
      continue

    indirect_sources_new = find_indirect_sources('\n'.join(f.NewContents()))
    if not indirect_sources_new:
      continue

    indirect_sources_old = find_indirect_sources('\n'.join(f.OldContents()))
    added_indirect_sources = (
        set(indirect_sources_new) - set(indirect_sources_old))

    if added_indirect_sources:
      results.append(output_api.PresubmitPromptWarning(
          "Indirect sources detected.",
          [f.LocalPath()],
          f"{MSG}\n  " + "\n  ".join(sorted(added_indirect_sources))))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(
    _CheckNoAutofillBrowserTestsWithoutAutofillBrowserTestEnvironment(
        input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(_RunHistogramChecks(input_api, output_api,
                 "BadMessageReasonChrome"))
  results.extend(_CheckNoInteractiveUiTestLibInNonInteractiveUiTest(
      input_api, output_api))
  results.extend(_CheckForUselessExterns(input_api, output_api))
  results.extend(_CheckBuildFilesForIndirectAshSources(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

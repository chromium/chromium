# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser code."""

import os
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


def _CheckAshSourcesForBadIncludes(input_api, output_api):
  """Make sure changes to Ash sources don't include c/b/ui/browser.h

  Intentionally not using BanRule as that may report includes as new that were
  already present.
  """

  MSG = ("Please don't add new #include's of chrome/browser/ui/browser.h to "
         "Ash code. Instead, use the BrowserDelegate/BrowserController "
         "abstraction in chrome/browser/ash/browser_delegate/ (preferred) or "
         "chrome/browser/ui/browser_window/public/browser_window_interface.h. "
         "If in doubt, please contact neis@google.com and hidehiko@google.com.")

  # If you add other files here, please adapt the message and comment above.
  bad_includes = [
      "chrome/browser/ui/browser.h",
  ]

  def should_check_path(affected_path):
    # TODO(crbug.com/447299513): Use pathlib's full_match once we are at Python
    # >= 3.13
    return (affected_path.startswith('chrome/browser/') and
            ('/ash/' in affected_path or '/chromeos/' in affected_path))

  bad_includes_re = re.compile(
      '|'.join(re.escape(f'#include "{file}"') for file in bad_includes)
  )

  def find_bad_includes(lines):
    return [line for line in lines if bad_includes_re.match(line)]

  results = []
  for f in input_api.AffectedTestableFiles():
    if not should_check_path(f.UnixLocalPath()):
      continue

    bad_includes_new = find_bad_includes(f.NewContents())
    if not bad_includes_new:
      continue

    bad_includes_old = find_bad_includes(f.OldContents())
    added_bad_includes = (
        set(bad_includes_new) - set(bad_includes_old))

    if added_bad_includes:
      results.append(output_api.PresubmitError(
          "Bad includes detected in the following files.",
          [f.LocalPath()],
          f"{MSG}\n"))
  return results


###############################################################################
# Check if all flag_descriptions are used from about_flags (cleanup)
###############################################################################

FLAG_DESCRIPTIONS  = 'chrome/browser/flag_descriptions.h'
ABOUT_FLAGS        = 'chrome/browser/about_flags.cc'
IDENTIFIER_FLAG_RE = re.compile(r'\bk[A-Z][A-Za-z0-9]+\b')
PREPROCESSOR_RE    = re.compile(r'^#if(?!ndef CHROME_BROWSER)', re.MULTILINE)

def _ReadFile(input_api, relpath: str):
  root = input_api.change.RepositoryRoot()
  abspath = os.path.join(root, relpath)
  with open(abspath, 'r', encoding='utf-8', errors='ignore') as f:
    return f.read()

def _NaiveExtractIdentifiers(text: str) -> set[str]:
    return set(IDENTIFIER_FLAG_RE.findall(text))

def _ReadIdentifiersFromFile(input_api, relpath: str):
  root = input_api.change.RepositoryRoot()
  abspath = os.path.join(root, relpath)
  with open(abspath, 'r', encoding='utf-8', errors='ignore') as f:
    return set(IDENTIFIER_FLAG_RE.findall(f.read()))

def _FlagFilesHaveChanged(input_api) -> bool:
  """ Detect if any of the files of interest have changed. """
  flag_files = {FLAG_DESCRIPTIONS, ABOUT_FLAGS}
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.LocalPath().replace('\\', '/') in flag_files:
      return True
  return False

def _CheckForUnwantedFlagDescriptionContent(input_api, output_api):
  result = []

  fd_content = _ReadFile(input_api, FLAG_DESCRIPTIONS)
  about_content = _ReadFile(input_api, ABOUT_FLAGS)

  fd_idents = _NaiveExtractIdentifiers(fd_content)
  about_idents = _NaiveExtractIdentifiers(about_content)

  redundant_idents = sorted(list(fd_idents - about_idents))
  if len(redundant_idents) > 0:
    result.append(output_api.PresubmitError(
        'The following flag_descriptions.h identifiers are no longer needed '
        'and should be removed:\n\t- ' + '\n\t- '.join(redundant_idents)))

  # Check for newly added #if(defined) -- can't have #else/#elif/#endif without
  # #if, so no need to look for that.
  if PREPROCESSOR_RE.search(fd_content):
    result.append(output_api.PresubmitError(
        'Preprocessor conditional directives should not be used in {}'.format(FLAG_DESCRIPTIONS)))

  # TODO: check if fd_flags are sorted.

  return result

###############################################################################
# Presubmit aggregator
###############################################################################

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
  results.extend(_CheckAshSourcesForBadIncludes(input_api, output_api))

  if _FlagFilesHaveChanged(input_api):
    results.extend(_CheckForUnwantedFlagDescriptionContent(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

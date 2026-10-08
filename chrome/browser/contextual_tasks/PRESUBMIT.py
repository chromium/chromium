# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for chrome/browser/contextual_tasks.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

_CONTEXTUAL_TASKS_UI_PATH = (
  r'^chrome[\\/]browser[\\/]contextual_tasks[\\/]contextual_tasks_ui\.cc$'
)

_BYPASS_FOOTER = 'Allow-Contextual-Tasks-Ui-Changes'

# Matches top-level C++ method or free-function definitions starting at column 0
# (under standard clang-format rules), whether the return type is on the same
# line (`void ContextualTasksUI::Foo(`) or wrapped onto the preceding line
# (`ContextualTasksUI::Foo(`).
_METHOD_DEF_PATTERN = (
  r'^(?:[\w:<>,\*&]+(?:[\s\*&]+[\w:<>,\*&]+)*\s+)?'
  r'((?:ContextualTasksUI(?:::\w+)*::)?~?\w+)\s*\('
)

# C++ keywords that could theoretically appear at column 0 before `(`.
_IGNORED_NAMES = frozenset(
  {
    'if',
    'for',
    'while',
    'switch',
    'catch',
    'return',
    'sizeof',
    'alignof',
    'decltype',
    'static_assert',
  }
)


def _ExtractMethodName(line, method_re, comment_re):
  """Extracts a top-level method/function name from a line, or None."""
  if comment_re.match(line):
    return None
  match = method_re.match(line)
  if not match:
    return None
  method_name = match.group(1)
  if method_name in _IGNORED_NAMES:
    return None
  # Ignore top-level macro invocations such as WEB_UI_CONTROLLER_TYPE_IMPL(
  # or DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(.
  if '::' not in method_name and method_name.isupper():
    return None
  return method_name


def _CountMethods(lines, method_re, comment_re):
  """Counts occurrences of each top-level method/function in `lines`."""
  counts = {}
  for line in lines:
    name = _ExtractMethodName(line, method_re, comment_re)
    if name:
      counts[name] = counts.get(name, 0) + 1
  return counts


def CheckNoNewMethodsInContextualTasksUi(input_api, output_api):
  """Warns when new methods are added to contextual_tasks_ui.cc."""
  git_footers = input_api.change.GitFootersFromDescription()
  for key, values in git_footers.items():
    if key.lower() == _BYPASS_FOOTER.lower():
      if any(v.strip() for v in values):
        return []

  file_filter = lambda f: input_api.FilterSourceFile(
    f, files_to_check=[_CONTEXTUAL_TASKS_UI_PATH]
  )
  affected_files = input_api.AffectedFiles(
    include_deletes=False, file_filter=file_filter
  )
  if not affected_files:
    return []

  method_re = input_api.re.compile(_METHOD_DEF_PATTERN)
  comment_re = input_api.re.compile(r'^\s*(//|/\*|\*)')

  new_methods = []
  for f in affected_files:
    old_counts = _CountMethods(f.OldContents(), method_re, comment_re)
    new_counts = _CountMethods(f.NewContents(), method_re, comment_re)
    reported_for_name = {}

    for line_num, line in f.ChangedContents():
      method_name = _ExtractMethodName(line, method_re, comment_re)
      if not method_name:
        continue

      # If OldContents() is populated and the total number of definitions
      # for `method_name` did not increase, this change only modified or
      # moved an existing method rather than adding a new one.
      allowed_new = new_counts.get(method_name, 0) - old_counts.get(
        method_name, 0
      )
      if f.OldContents() and allowed_new <= reported_for_name.get(
        method_name, 0
      ):
        continue

      reported_for_name[method_name] = reported_for_name.get(method_name, 0) + 1
      new_methods.append(f'  {f.LocalPath()}:{line_num}: {method_name}()')

  if not new_methods:
    return []

  return [
    output_api.PresubmitPromptWarning(
      'Do not add new methods or functions to contextual_tasks_ui.cc. '
      'ContextualTasksUI is deprecated and will be removed once the '
      'Contextual Tasks rearchitecture launches. Instead, add shared '
      'toolbar/page methods to ContextualTasksUIBase '
      '(contextual_tasks_ui_base.cc) or post-rearchitecture-specific '
      'methods to ContextualTasksUIPostRearchitecture '
      '(contextual_tasks_ui_post_rearchitecture.cc) '
      f'(add "{_BYPASS_FOOTER}: <reason>" to the commit description '
      'footers if an exception is strictly necessary):',
      new_methods,
    )
  ]

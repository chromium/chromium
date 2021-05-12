# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks protobuf files for illegal imports."""

import codecs
import os
import re

import results
from rules import Rule, MessageRule


class ProtoChecker(object):

  EXTENSIONS = [
      '.proto',
  ]

  # The maximum number of non-import lines we can see before giving up.
  _MAX_UNINTERESTING_LINES = 50

  # The maximum line length, this is to be efficient in the case of very long
  # lines (which can't be import).
  _MAX_LINE_LENGTH = 128

  # This regular expression will be used to extract filenames from import
  # statements.
  _EXTRACT_IMPORT_PATH = re.compile(
      '[ \t]*[ \t]*import[ \t]+"(.*)"')

  def __init__(self, verbose, resolve_dotdot=False, root_dir=''):
    self._verbose = verbose
    self._resolve_dotdot = resolve_dotdot
    self._root_dir = root_dir

  def IsFullPath(self, import_path):
    """Checks if the given path is a valid path starting from |_root_dir|."""
    match = re.match('(.*)/([^/]*\.proto)', import_path)
    if not match:
      return False
    return os.path.isdir(self._root_dir + "/" + match.group(1))

  def CheckLine(self, rules, line, dependee_path, fail_on_temp_allow=False):
    """Checks the given line with the given rule set.

    Returns a tuple (is_import, dependency_violation) where
    is_import is True only if the line is an import
    statement, and dependency_violation is an instance of
    results.DependencyViolation if the line violates a rule, or None
    if it does not.
    """
    found_item = self._EXTRACT_IMPORT_PATH.match(line)
    if not found_item:
      return False, None  # Not a match

    import_path = found_item.group(1)

    if '\\' in import_path:
      return True, results.DependencyViolation(
          import_path,
          MessageRule('Import paths may not include backslashes.'),
          rules)

    if '/' not in import_path:
      # Don't fail when no directory is specified. We may want to be more
      # strict about this in the future.
      if self._verbose:
        print ' WARNING: import specified with no directory: ' + import_path
      return True, None

    if self._resolve_dotdot and '../' in import_path:
      dependee_dir = os.path.dirname(dependee_path)
      import_path = os.path.join(dependee_dir, import_path)
      import_path = os.path.relpath(import_path, self._root_dir)

    if not self.IsFullPath(import_path):
      return True, None

    rule = rules.RuleApplyingTo(import_path, dependee_path)

    if (rule.allow == Rule.DISALLOW or
        (fail_on_temp_allow and rule.allow == Rule.TEMP_ALLOW)):
      return True, results.DependencyViolation(import_path, rule, rules)
    return True, None

  def CheckFile(self, rules, filepath):
    if self._verbose:
      print 'Checking: ' + filepath

    dependee_status = results.DependeeStatus(filepath)
    last_import = 0
    with codecs.open(filepath, encoding='utf-8') as f:
      for line_num, line in enumerate(f):
        if line_num - last_import > self._MAX_UNINTERESTING_LINES:
          break

        line = line.strip()

        is_import, violation = self.CheckLine(rules, line, filepath)
        if is_import:
          last_import = line_num
        if violation:
          dependee_status.AddViolation(violation)

    return dependee_status

  @staticmethod
  def IsProtoFile(file_path):
    """Returns True iff the given path ends in one of the extensions
    handled by this checker.
    """
    return os.path.splitext(file_path)[1] in ProtoChecker.EXTENSIONS

  def ShouldCheck(self, file_path):
    """Check if the new #include file path should be presubmit checked.

    Args:
      file_path: file path to be checked

    Return:
      bool: True if the file should be checked; False otherwise.
    """
    return self.IsProtoFile(file_path)

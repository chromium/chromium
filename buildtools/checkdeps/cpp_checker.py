# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks C++ and Objective-C files for illegal includes."""

import os
import re

import results
from rules import Rule, MessageRule

_BACKSLASH_RULE = MessageRule('Include paths may not include backslashes.')


class CppChecker(object):
  EXTENSIONS = [
    '.h',
    '.cc',
    '.cpp',
    '.m',
    '.mm',
  ]

  # This regular expression will be used to extract filenames from include
  # statements.
  _EXTRACT_INCLUDE_PATH = re.compile(
    r'[ \t]*#[ \t]*(?:include|import)[ \t]*"(.*)"'
  )

  # CheckFile() scans a whole file with one regex over its raw bytes instead
  # of decoding it and matching line by line. For includes this is the bytes
  # equivalent of stripping a line and applying _EXTRACT_INCLUDE_PATH: like
  # `(.*)` there, the greedy `[^\r\n]*` captures up to the last '"' on the
  # line. The regex also reports the directives needed to skip `#if 0` blocks;
  # `#if` matches `#ifdef` and `#ifndef` too, which nest inside such a block.
  _BYTES_IF0_OR_INCLUDE_RE = re.compile(
    rb'^[ \t\v\f]*(?:'
    rb'#(if 0|if|endif)'
    rb'|#[ \t]*(?:include|import)[ \t]*"([^\r\n]*)"'
    rb')',
    re.MULTILINE,
  )

  def __init__(self, verbose, resolve_dotdot=False, root_dir=''):
    self._verbose = verbose
    self._resolve_dotdot = resolve_dotdot
    self._root_dir = root_dir

  def _CheckIncludePath(
    self,
    rules,
    include_path,
    dependee_path,
    matched_specific,
    fail_on_temp_allow=False,
  ):
    """Returns a DependencyViolation if |include_path| is not allowed.

    |matched_specific| is rules.GetSpecificRulesForFile() for the dependee.
    """
    if '\\' in include_path:
      return results.DependencyViolation(include_path, _BACKSLASH_RULE, rules)

    if '/' not in include_path:
      # Don't fail when no directory is specified. We may want to be more
      # strict about this in the future.
      if self._verbose:
        print(' WARNING: include specified with no directory: ' + include_path)
      return None

    if self._resolve_dotdot and '../' in include_path:
      dependee_dir = os.path.dirname(dependee_path)
      include_path = os.path.join(dependee_dir, include_path)
      include_path = os.path.relpath(include_path, self._root_dir)
      # Normalize to use forward slashes, since all rules are specified
      # in terms of forward slashes.
      include_path = include_path.replace(os.path.sep, '/')

    rule = rules.RuleApplyingToWithSpecific(include_path, matched_specific)
    if rule.allow == Rule.DISALLOW or (
      fail_on_temp_allow and rule.allow == Rule.TEMP_ALLOW
    ):
      return results.DependencyViolation(include_path, rule, rules)
    return None

  def CheckLine(self, rules, line, dependee_path, fail_on_temp_allow=False):
    """Checks the given line with the given rule set.

    Returns a tuple (is_include, dependency_violation) where
    is_include is True only if the line is an #include or #import
    statement, and dependency_violation is an instance of
    results.DependencyViolation if the line violates a rule, or None
    if it does not.
    """
    found_item = self._EXTRACT_INCLUDE_PATH.match(line)
    if not found_item:
      return False, None  # Not a match

    include_path = found_item.group(1)
    matched_specific = rules.GetSpecificRulesForFile(
      os.path.basename(dependee_path)
    )
    violation = self._CheckIncludePath(
      rules, include_path, dependee_path, matched_specific, fail_on_temp_allow
    )
    return True, violation

  def CheckFile(self, rules, filepath):
    if self._verbose:
      print('Checking: ' + filepath)

    dependee_status = results.DependeeStatus(filepath)
    with open(filepath, 'rb') as f:
      data = f.read()

    if b'#' not in data:
      return dependee_status

    matched_specific = rules.GetSpecificRulesForFile(os.path.basename(filepath))

    # Nesting depth of conditionals, counted from the outermost `#if 0`.
    in_if0 = 0
    for match in self._BYTES_IF0_OR_INCLUDE_RE.finditer(data):
      directive = match.group(1)
      if directive is not None:
        if directive == b'if 0':
          in_if0 += 1
        elif in_if0 > 0:
          if directive == b'if':
            in_if0 += 1
          elif directive == b'endif':
            in_if0 -= 1
      elif in_if0 == 0:
        include_path = match.group(2).decode('utf-8')
        violation = self._CheckIncludePath(
          rules, include_path, filepath, matched_specific
        )
        if violation:
          dependee_status.AddViolation(violation)

    return dependee_status

  @staticmethod
  def IsCppFile(file_path):
    """Returns True iff the given path ends in one of the extensions
    handled by this checker.
    """
    return os.path.splitext(file_path)[1] in CppChecker.EXTENSIONS

  def ShouldCheck(self, file_path):
    """Check if the new #include file path should be presubmit checked.

    Args:
      file_path: file path to be checked

    Return:
      bool: True if the file should be checked; False otherwise.
    """
    return self.IsCppFile(file_path)

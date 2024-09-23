# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys


def GetScriptName():
  return os.path.basename(os.path.abspath(sys.argv[0]))


def GetJavaFilePath(java_package, class_name):
  package_path = java_package.replace('.', os.path.sep)
  file_name = class_name + '.java'
  return os.path.join(package_path, file_name)


def KCamelToShouty(s):
  """Convert |s| from kCamelCase or CamelCase to SHOUTY_CASE.

  kFooBar -> FOO_BAR
  FooBar -> FOO_BAR
  FooBAR9 -> FOO_BAR9
  FooBARBaz -> FOO_BAR_BAZ
  """
  if not re.match(r'^k?([A-Z][^A-Z]+|[A-Z0-9]+)+$', s):
    return s
  # Strip the leading k.
  s = re.sub(r'^k', '', s)
  # Treat "WebView" like one word.
  s = re.sub(r'WebView', r'Webview', s)
  # Add _ between title words and anything else.
  s = re.sub(r'([^_])([A-Z][^A-Z_0-9]+)', r'\1_\2', s)
  # Add _ between lower -> upper transitions.
  s = re.sub(r'([^A-Z_0-9])([A-Z])', r'\1_\2', s)
  return s.upper()


class JavaString:
  def __init__(self, name, value, comments):
    self.name = KCamelToShouty(name)
    self.value = value
    self.comments = '\n'.join('    ' + x for x in comments)

  def Format(self):
    return '%s\n    public static final String %s = %s;' % (
        self.comments, self.name, self.value)


def ParseTemplateFile(lines):
  package_re = re.compile(r'^package (.*);')
  class_re = re.compile(r'.*class (.*) {')
  package = ''
  class_name = ''
  for line in lines:
    package_line = package_re.match(line)
    if package_line:
      package = package_line.groups()[0]
    class_line = class_re.match(line)
    if class_line:
      class_name = class_line.groups()[0]
      break
  return package, class_name


# TODO(crbug.com/40616187): Work will be needed if we want to annotate specific
# constants in the file to be parsed.
class CppConstantParser:
  """Parses C++ constants, retaining their comments.

  The Delegate subclass is responsible for matching and extracting the
  constant's variable name and value, as well as generating an object to
  represent the Java representation of this value.
  """
  SINGLE_LINE_COMMENT_RE = re.compile(r'\s*(// [^\n]*)')

  class Delegate:
    def ExtractConstantName(self, line):
      """Extracts a constant's name from line or None if not a match."""
      raise NotImplementedError()

    def ExtractValue(self, line):
      """Extracts a constant's value from line or None if not a match."""
      raise NotImplementedError()

    def CreateJavaConstant(self, name, value, comments):
      """Creates an object representing the Java analog of a C++ constant.

      CppConstantParser will not interact with the object created by this
      method. Instead, it will store this value in a list and return a list of
      all objects from the Parse() method. In this way, the caller may define
      whatever class suits their need.

      Args:
        name: the constant's variable name, as extracted by
          ExtractConstantName()
        value: the constant's value, as extracted by ExtractValue()
        comments: the code comments describing this constant
      """
      raise NotImplementedError()

  def __init__(self, delegate, lines):
    self._delegate = delegate
    self._lines = lines
    self._in_variable = False
    self._in_comment = False
    self._package = ''
    self._current_comments = []
    self._current_name = ''
    self._current_value = ''
    self._constants = []

  def _ExtractVariable(self, line):
    match = StringFileParser.STRING_RE.match(line)
    return match.group(1) if match else None

  def _ExtractValue(self, line):
    match = StringFileParser.VALUE_RE.search(line)
    return match.group(1) if match else None

  def _Reset(self):
    self._current_comments = []
    self._current_name = ''
    self._current_value = ''
    self._in_variable = False
    self._in_comment = False

  def _AppendConstant(self):
    self._constants.append(
        self._delegate.CreateJavaConstant(self._current_name,
                                          self._current_value,
                                          self._current_comments))
    self._Reset()

  def _ParseValue(self, line):
    current_value = self._delegate.ExtractValue(line)
    if current_value is not None:
      self._current_value = current_value
      self._AppendConstant()
    else:
      self._Reset()

  def _ParseComment(self, line):
    comment_line = CppConstantParser.SINGLE_LINE_COMMENT_RE.match(line)
    if comment_line:
      self._current_comments.append(comment_line.groups()[0])
      self._in_comment = True
      self._in_variable = True
      return True
    self._in_comment = False
    return False

  def _ParseVariable(self, line):
    current_name = self._delegate.ExtractConstantName(line)
    if current_name is not None:
      self._current_name = current_name
      current_value = self._delegate.ExtractValue(line)
      if current_value is not None:
        self._current_value = current_value
        self._AppendConstant()
      else:
        self._in_variable = True
      return True
    self._in_variable = False
    return False

  def _ParseLine(self, line):
    if not self._in_variable:
      if not self._ParseVariable(line):
        self._ParseComment(line)
      return

    if self._in_comment:
      if self._ParseComment(line):
        return
      if not self._ParseVariable(line):
        self._Reset()
      return

    if self._in_variable:
      self._ParseValue(line)

  def Parse(self):
    """Returns a list of objects representing C++ constants.

    Each object in the list was created by Delegate.CreateJavaValue().
    """
    for line in self._lines:
      self._ParseLine(line)
    return self._constants

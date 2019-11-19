# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import json
import os
import re
import subprocess
import sys

# TODO(dcheng): It's kind of horrible that this is copy and pasted from
# presubmit_canned_checks.py, but it's far easier than any of the alternatives.
def _ReportErrorFileAndLine(filename, line_num, dummy_line):
  """Default error formatter for _FindNewViolationsOfRule."""
  return '%s:%s' % (filename, line_num)


class MockCannedChecks(object):
  def _FindNewViolationsOfRule(self, callable_rule, input_api,
                               source_file_filter=None,
                               error_formatter=_ReportErrorFileAndLine):
    """Find all newly introduced violations of a per-line rule (a callable).

    Arguments:
      callable_rule: a callable taking a file extension and line of input and
        returning True if the rule is satisfied and False if there was a
        problem.
      input_api: object to enumerate the affected files.
      source_file_filter: a filter to be passed to the input api.
      error_formatter: a callable taking (filename, line_number, line) and
        returning a formatted error string.

    Returns:
      A list of the newly-introduced violations reported by the rule.
    """
    errors = []
    for f in input_api.AffectedFiles(include_deletes=False,
                                     file_filter=source_file_filter):
      # For speed, we do two passes, checking first the full file.  Shelling out
      # to the SCM to determine the changed region can be quite expensive on
      # Win32.  Assuming that most files will be kept problem-free, we can
      # skip the SCM operations most of the time.
      extension = str(f.LocalPath()).rsplit('.', 1)[-1]
      if all(callable_rule(extension, line) for line in f.NewContents()):
        continue  # No violation found in full text: can skip considering diff.

      for line_num, line in f.ChangedContents():
        if not callable_rule(extension, line):
          errors.append(error_formatter(f.LocalPath(), line_num, line))

    return errors


class MockInputApi(object):
  """Mock class for the InputApi class.

  This class can be used for unittests for presubmit by initializing the files
  attribute as the list of changed files.
  """

  DEFAULT_BLACK_LIST = ()

  def __init__(self):
    self.canned_checks = MockCannedChecks()
    self.fnmatch = fnmatch
    self.json = json
    self.re = re
    self.os_path = os.path
    self.platform = sys.platform
    self.python_executable = sys.executable
    self.platform = sys.platform
    self.subprocess = subprocess
    self.files = []
    self.is_committing = False
    self.change = MockChange([])
    self.presubmit_local_path = os.path.dirname(__file__)

  def CreateMockFileInPath(self, f_list):
    self.os_path.exists = lambda x: x in f_list

  def AffectedFiles(self, file_filter=None, include_deletes=False):
    for file in self.files:
      if file_filter and not file_filter(file):
        continue
      if not include_deletes and file.Action() == 'D':
        continue
      yield file

  def AffectedSourceFiles(self, file_filter=None):
    return self.AffectedFiles(file_filter=file_filter)

  def FilterSourceFile(self, file, white_list=(), black_list=()):
    local_path = file.LocalPath()
    found_in_white_list = not white_list
    if white_list:
      if type(white_list) is str:
        raise TypeError('white_list should be an iterable of strings')
      for pattern in white_list:
        compiled_pattern = re.compile(pattern)
        if compiled_pattern.search(local_path):
          found_in_white_list = True
          break
    if black_list:
      if type(black_list) is str:
        raise TypeError('black_list should be an iterable of strings')
      for pattern in black_list:
        compiled_pattern = re.compile(pattern)
        if compiled_pattern.search(local_path):
          return False
    return found_in_white_list

  def LocalPaths(self):
    return [file.LocalPath() for file in self.files]

  def PresubmitLocalPath(self):
    return self.presubmit_local_path

  def ReadFile(self, filename, mode='rU'):
    if hasattr(filename, 'AbsoluteLocalPath'):
       filename = filename.AbsoluteLocalPath()
    for file_ in self.files:
      if file_.LocalPath() == filename:
        return '\n'.join(file_.NewContents())
    # Otherwise, file is not in our mock API.
    raise IOError, "No such file or directory: '%s'" % filename


class MockOutputApi(object):
  """Mock class for the OutputApi class.

  An instance of this class can be passed to presubmit unittests for outputing
  various types of results.
  """

  class PresubmitResult(object):
    def __init__(self, message, items=None, long_text=''):
      self.message = message
      self.items = items
      self.long_text = long_text

    def __repr__(self):
      return self.message

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'error'

  class PresubmitPromptWarning(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'warning'

  class PresubmitNotifyResult(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'notify'

  class PresubmitPromptOrNotify(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'promptOrNotify'

  def __init__(self):
    self.more_cc = []

  def AppendCC(self, more_cc):
    self.more_cc.extend(more_cc)


class MockFile(object):
  """Mock class for the File class.

  This class can be used to form the mock list of changed files in
  MockInputApi for presubmit unittests.
  """

  def __init__(self, local_path, new_contents, old_contents=None, action='A'):
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_contents = [(i + 1, l) for i, l in enumerate(new_contents)]
    self._action = action
    self._scm_diff = "--- /dev/null\n+++ %s\n@@ -0,0 +1,%d @@\n" % (local_path,
      len(new_contents))
    self._old_contents = old_contents
    for l in new_contents:
      self._scm_diff += "+%s\n" % l

  def Action(self):
    return self._action

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path

  def AbsoluteLocalPath(self):
    return self._local_path

  def GenerateScmDiff(self):
    return self._scm_diff

  def OldContents(self):
    return self._old_contents

  def rfind(self, p):
    """os.path.basename is called on MockFile so we need an rfind method."""
    return self._local_path.rfind(p)

  def __getitem__(self, i):
    """os.path.basename is called on MockFile so we need a get method."""
    return self._local_path[i]

  def __len__(self):
    """os.path.basename is called on MockFile so we need a len method."""
    return len(self._local_path)

  def replace(self, altsep, sep):
    """os.path.basename is called on MockFile so we need a replace method."""
    return self._local_path.replace(altsep, sep)


class MockAffectedFile(MockFile):
  def AbsoluteLocalPath(self):
    return self._local_path


class MockChange(object):
  """Mock class for Change class.

  This class can be used in presubmit unittests to mock the query of the
  current change.
  """

  def __init__(self, changed_files):
    self._changed_files = changed_files

  def LocalPaths(self):
    return self._changed_files

  def AffectedFiles(self, include_dirs=False, include_deletes=True,
                    file_filter=None):
    return self._changed_files

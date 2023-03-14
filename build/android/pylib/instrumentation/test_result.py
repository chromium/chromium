# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from pylib.base import base_test_result

# This must match the source adding the suffix: bit.ly/3Zmwwyx
_MULTIPROCESS_SUFFIX = '__multiprocess_mode'


class InstrumentationTestResult(base_test_result.BaseTestResult):
  """Result information for a single instrumentation test."""

  def __init__(self, full_name, test_type, dur, log=''):
    """Construct an InstrumentationTestResult object.

    Args:
      full_name: Full name of the test.
      test_type: Type of the test result as defined in ResultType.
      dur: Duration of the test run in milliseconds.
      log: A string listing any errors.
    """
    super().__init__(full_name, test_type, dur, log)
    name_pieces = full_name.rsplit('#')
    if len(name_pieces) > 1:
      self._test_name = name_pieces[1]
      self._class_name = name_pieces[0]
    else:
      self._class_name = full_name
      self._test_name = full_name

    self._webview_multiprocess_mode = full_name.endswith(_MULTIPROCESS_SUFFIX)

  def SetDuration(self, duration):
    """Set the test duration."""
    self._duration = duration

  def GetNameForResultSink(self):
    """Get the test name to be reported to resultsink."""
    raw_name = self.GetName()
    if self._webview_multiprocess_mode:
      assert raw_name.endswith(
          _MULTIPROCESS_SUFFIX
      ), 'multiprocess mode test raw name should have the corresponding suffix'
      return raw_name[:-len(_MULTIPROCESS_SUFFIX)]
    return raw_name

  def GetVariantForResultSink(self):
    """Get the variant dict to be reported to resultsink."""
    if self._webview_multiprocess_mode:
      return {'webview_multiprocess_mode': 'Yes'}
    return None

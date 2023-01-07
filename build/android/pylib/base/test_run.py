# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class TestRun:
  """An execution of a particular test on a particular device.

  This is expected to handle all logic that is specific to the combination of
  environment and test type.

  Examples include:
    - local gtests
    - local instrumentation tests
  """

  def __init__(self, env, test_instance):
    self._env = env
    self._test_instance = test_instance

    # Some subclasses have different teardown behavior on receiving SIGTERM.
    self._received_sigterm = False

  def TestPackage(self):
    raise NotImplementedError

  def SetUp(self):
    raise NotImplementedError

  def RunTests(self, results, raw_logs_fh=None):
    """Runs Tests and populates |results|.

    Args:
      results: An array that should be populated with
               |base_test_result.TestRunResults| objects.
      raw_logs_fh: An optional file handle to write raw logs to.
    """
    raise NotImplementedError

  def GetTestsForListing(self):
    """Returns a list of test names."""
    raise NotImplementedError

  def TearDown(self):
    raise NotImplementedError

  def __enter__(self):
    self.SetUp()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.TearDown()

  def ReceivedSigterm(self):
    self._received_sigterm = True

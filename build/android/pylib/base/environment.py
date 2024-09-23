# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# TODO(crbug.com/40799394): After Telemetry is supported by python3 we can
# remove object inheritance from this script.
# pylint: disable=useless-object-inheritance
class Environment(object):
  """An environment in which tests can be run.

  This is expected to handle all logic that is applicable to an entire specific
  environment but is independent of the test type.

  Examples include:
    - The local device environment, for running tests on devices attached to
      the local machine.
    - The local machine environment, for running tests directly on the local
      machine.
  """

  def __init__(self, output_manager):
    """Environment constructor.

    Args:
      output_manager: Instance of |output_manager.OutputManager| used to
          save test output.
    """
    self._output_manager = output_manager

    # Some subclasses have different teardown behavior on receiving SIGTERM.
    self._received_sigterm = False

  def SetUp(self):
    raise NotImplementedError

  def TearDown(self):
    raise NotImplementedError

  def __enter__(self):
    self.SetUp()
    return self

  def __exit__(self, _exc_type, _exc_val, _exc_tb):
    self.TearDown()

  @property
  def output_manager(self):
    return self._output_manager

  def ReceivedSigterm(self):
    self._received_sigterm = True

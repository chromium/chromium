#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests scenarios for ermine_ctl"""
import common
import os
import logging
import subprocess
import time
import unittest
import unittest.mock as mock
from target import Target
from ermine_ctl import ErmineCtl


class TestDiscoverDeviceTarget(unittest.TestCase):
  def setUp(self):
    self.mock_target = mock.create_autospec(Target, instance=True)
    self.ermine_ctl = ErmineCtl(self.mock_target)

  def testCheckExists(self):
    self.mock_target.RunCommand.return_value = 0

    self.assertFalse(self.ermine_ctl._ermine_exists)
    self.assertFalse(self.ermine_ctl._ermine_exists_check)

    self.assertTrue(self.ermine_ctl.exists)

    self.assertTrue(self.ermine_ctl._ermine_exists)
    self.assertTrue(self.ermine_ctl._ermine_exists_check)

    # Modifying this will not result in a change in state.
    self.mock_target.RunCommand.return_value = 42
    self.assertTrue(self.ermine_ctl.exists)
    self.assertTrue(self.ermine_ctl._ermine_exists)
    self.assertTrue(self.ermine_ctl._ermine_exists_check)

  def testDoesNotExist(self):
    self.mock_target.RunCommand.return_value = 42
    self.assertFalse(self.ermine_ctl._ermine_exists)
    self.assertFalse(self.ermine_ctl._ermine_exists_check)

    self.assertFalse(self.ermine_ctl.exists)
    self.assertFalse(self.ermine_ctl._ermine_exists)
    self.assertTrue(self.ermine_ctl._ermine_exists_check)

  def testReadyReturnsFalseIfNoExist(self):
    self.ermine_ctl._ermine_exists = False
    self.ermine_ctl._ermine_exists_check = True

    self.assertFalse(self.ermine_ctl.ready)

  def testReadyReturnsFalseIfBadStatus(self):
    with mock.patch.object(
        ErmineCtl, 'status', new_callable=mock.PropertyMock) as mock_status, \
      mock.patch.object(ErmineCtl, 'exists',
                        new_callable=mock.PropertyMock) as mock_exists:
      mock_exists.return_value = True
      mock_status.return_value = (1, 'FakeStatus')
      self.assertFalse(self.ermine_ctl.ready)

  def testReadyReturnsTrue(self):
    with mock.patch.object(
        ErmineCtl, 'status', new_callable=mock.PropertyMock) as mock_status, \
      mock.patch.object(ErmineCtl, 'exists',
                        new_callable=mock.PropertyMock) as mock_exists:
      mock_exists.return_value = True
      mock_status.return_value = (0, 'FakeStatus')
      self.assertTrue(self.ermine_ctl.ready)

  def testStatusReturnsInvalidStateIfNoExists(self):
    with mock.patch.object(ErmineCtl, 'exists',
                           new_callable=mock.PropertyMock) as mock_exists:
      mock_exists.return_value = False

      self.assertEqual(self.ermine_ctl.status, (-2, 'InvalidState'))

  def testStatusReturnsRCAndStdout(self):
    with mock.patch.object(
        ErmineCtl, 'exists', new_callable=mock.PropertyMock) as mock_exists, \
            mock.patch.object(
                ErmineCtl, '_ExecuteCommandAsync') as mock_execute:
      mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
      mock_proc.communicate.return_value = 'foo', 'stderr'
      mock_proc.returncode = 10
      mock_execute.return_value = mock_proc

      self.assertEqual(self.ermine_ctl.status, (10, 'foo'))

  def testStatusReturnsTimeoutState(self):
    with mock.patch.object(
        ErmineCtl, 'exists', new_callable=mock.PropertyMock) as mock_exists, \
            mock.patch.object(
                ErmineCtl, '_ExecuteCommandAsync') as mock_execute, \
        mock.patch.object(logging, 'warning') as _:
      mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
      mock_proc.wait.side_effect = subprocess.TimeoutExpired(
          'cmd', 'some timeout')
      mock_execute.return_value = mock_proc

      self.assertEqual(self.ermine_ctl.status, (-1, 'Timeout'))

  def testExecuteCommandRaisesErrorOnNonZeroReturn(self):
    with mock.patch.object(ErmineCtl, '_ExecuteCommandAsync') as mock_execute:
      mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
      mock_proc.returncode = 42
      mock_proc.communicate.return_value = ('foo', 'bar')
      mock_execute.return_value = mock_proc

      self.assertRaises(RuntimeError, self.ermine_ctl._ExecuteCommand,
                        ['some', 'command', '--args'])

      mock_execute.assert_called_once_with(['some', 'command', '--args'])

      mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
      mock_proc.returncode = 0
      mock_proc.communicate.return_value = ('foo', 'bar')
      mock_execute.return_value = mock_proc

      self.assertIsNone(self.ermine_ctl._ExecuteCommand(['another', 'command']))

  def testExecuteCommandAsync(self):
    mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
    self.mock_target.RunCommandPiped.return_value = mock_proc
    self.ermine_ctl._TOOL = 'foo'
    self.ermine_ctl._OOBE_SUBTOOL = 'oobe_buzz'

    self.assertEqual(
        self.ermine_ctl._ExecuteCommandAsync(['some', 'command', '--args']),
        mock_proc)

    self.mock_target.RunCommandPiped.assert_called_once_with(
        ['foo', 'oobe_buzz', 'some', 'command', '--args'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True)
    self.assertEqual(self.mock_target.RunCommand.call_count, 0)

  def testWaitUntilReadyIsNotReadyIfDNE(self):
    with mock.patch.object(ErmineCtl, 'exists',
                           new_callable=mock.PropertyMock) as mock_exists:
      mock_exists.return_value = False

      self.assertFalse(self.ermine_ctl.WaitUntilReady())

  def testWaitUntilReadyLoopsUntilReady(self):
    with mock.patch.object(ErmineCtl, 'exists',
                           new_callable=mock.PropertyMock) as mock_exists, \
        mock.patch.object(time, 'sleep') as mock_sleep, \
        mock.patch.object(ErmineCtl, 'ready',
                          new_callable=mock.PropertyMock) as mock_ready:
      mock_exists.return_value = True
      mock_ready.side_effect = [False, False, False, True]

      self.assertTrue(self.ermine_ctl.WaitUntilReady())

      self.assertEqual(mock_ready.call_count, 4)
      self.assertEqual(mock_sleep.call_count, 3)

  def testWaitUntilReadyLoopsUntilReady(self):
    with mock.patch.object(ErmineCtl, 'exists',
                           new_callable=mock.PropertyMock) as mock_exists, \
        mock.patch.object(time, 'sleep') as mock_sleep, \
        mock.patch.object(ErmineCtl, 'ready',
                          new_callable=mock.PropertyMock) as mock_ready:
      mock_exists.return_value = True
      mock_ready.side_effect = [False, False, False, True]
      self.ermine_ctl._WAIT_ATTEMPTS = 3

      self.assertRaises(TimeoutError, self.ermine_ctl.WaitUntilReady)

      self.assertEqual(mock_ready.call_count, 3)
      self.assertEqual(mock_sleep.call_count, 3)

  def testTakeToShellAssertsReady(self):
    with mock.patch.object(ErmineCtl, 'WaitUntilReady') as mock_wait_ready:
      mock_wait_ready.return_value = False
      self.assertRaises(AssertionError, self.ermine_ctl.TakeToShell)

  def testTakeToShellExitsOnCompleteState(self):
    with mock.patch.object(ErmineCtl, 'WaitUntilReady') as mock_wait_ready, \
        mock.patch.object(
            ErmineCtl, 'status',
            new_callable=mock.PropertyMock) as mock_status, \
        mock.patch.object(ErmineCtl, '_ExecuteCommand') as mock_execute:
      mock_wait_ready.return_value = True
      mock_status.return_value = (0, 'SomeCompleteState')
      self.ermine_ctl._COMPLETE_STATE = 'SomeCompleteState'

      self.ermine_ctl.TakeToShell()

      self.assertEqual(mock_execute.call_count, 0)

  def testTakeToShellInvalidStateRaisesException(self):
    with mock.patch.object(ErmineCtl, 'WaitUntilReady') as mock_wait_ready, \
        mock.patch.object(
            ErmineCtl, 'status',
            new_callable=mock.PropertyMock) as mock_status, \
        mock.patch.object(ErmineCtl, '_ExecuteCommand') as mock_execute:
      mock_wait_ready.return_value = True
      mock_status.return_value = (0, 'SomeUnknownState')

      self.assertNotIn('SomeUnknownState', self.ermine_ctl._STATE_TO_NEXT)

      self.assertRaises(NotImplementedError, self.ermine_ctl.TakeToShell)

  def testTakeToShellWithMaxTransitionsRaisesError(self):
    with mock.patch.object(ErmineCtl, 'WaitUntilReady') as mock_wait_ready, \
        mock.patch.object(
            ErmineCtl, 'status',
            new_callable=mock.PropertyMock) as mock_status, \
        mock.patch.object(ErmineCtl, '_ExecuteCommand') as mock_execute:
      mock_wait_ready.return_value = True
      self.ermine_ctl._MAX_STATE_TRANSITIONS = 3
      # Returns too many state transitions before CompleteState.
      mock_status.side_effect = [(0, 'Unknown'), (0, 'KnownWithPassword'),
                                 (0, 'Unknown'), (0, 'KnownWithPassword'),
                                 (0, 'CompleteState')]
      self.ermine_ctl._COMPLETE_STATE = 'CompleteState'
      self.ermine_ctl._STATE_TO_NEXT = {
          'Unknown': 'fizz',
          'KnownWithPassword': 'buzz',
          'CompleteState': 'do nothing'
      }

      self.assertRaises(RuntimeError, self.ermine_ctl.TakeToShell)

  def testTakeToShellExecutesKnownCommands(self):
    with mock.patch.object(ErmineCtl, 'WaitUntilReady') as mock_wait_ready, \
        mock.patch.object(
            ErmineCtl, 'status',
            new_callable=mock.PropertyMock) as mock_status, \
        mock.patch.object(ErmineCtl, '_ExecuteCommand') as mock_execute:
      mock_wait_ready.return_value = True
      mock_status.side_effect = [(0, 'Unknown'), (0, 'KnownWithPassword'),
                                 (0, 'CompleteState')]
      self.ermine_ctl._COMPLETE_STATE = 'CompleteState'
      self.ermine_ctl._STATE_TO_NEXT = {
          'Unknown': ['fizz'],
          'KnownWithPassword': ['buzz', 'super plaintext password'],
          'CompleteState': ['do nothing']
      }

      self.ermine_ctl.TakeToShell()

      self.assertEqual(mock_execute.call_count, 2)
      mock_execute.assert_has_calls([
          mock.call(['fizz']),
          mock.call(['buzz', 'super plaintext password'])
      ])


if __name__ == '__main__':
  unittest.main()

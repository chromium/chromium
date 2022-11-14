#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests scenarios for ermine_ctl"""
import logging
import subprocess
import time
import unittest
import unittest.mock as mock

from base_ermine_ctl import BaseErmineCtl


class BaseBaseErmineCtlTest(unittest.TestCase):
    """Unit tests for BaseBaseErmineCtl interface."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.ermine_ctl = BaseErmineCtl()

    def _set_mock_proc(self, return_value: int):
        """Set |execute_command_async|'s return value to a mocked subprocess."""
        self.ermine_ctl.execute_command_async = mock.MagicMock()
        mock_proc = mock.create_autospec(subprocess.Popen, instance=True)
        mock_proc.communicate.return_value = 'foo', 'stderr'
        mock_proc.returncode = return_value
        self.ermine_ctl.execute_command_async.return_value = mock_proc

        return mock_proc

    def test_check_exists(self):
        """Test |exists| returns True if tool command succeeds (returns 0)."""
        self._set_mock_proc(return_value=0)

        self.assertTrue(self.ermine_ctl.exists)

        # Modifying this will not result in a change in state due to caching.
        self._set_mock_proc(return_value=42)
        self.assertTrue(self.ermine_ctl.exists)

    def test_does_not_exist(self):
        """Test |exists| returns False if tool command fails (returns != 0)."""
        self._set_mock_proc(return_value=42)

        self.assertFalse(self.ermine_ctl.exists)

    def test_ready_raises_assertion_error_if_not_exist(self):
        """Test |ready| raises AssertionError if tool does not exist."""
        self._set_mock_proc(return_value=42)
        self.assertRaises(AssertionError, getattr, self.ermine_ctl, 'ready')

    def test_ready_returns_false_if_bad_status(self):
        """Test |ready| return False if tool has a bad status."""
        with mock.patch.object(
                BaseErmineCtl, 'status',
                new_callable=mock.PropertyMock) as mock_status, \
            mock.patch.object(BaseErmineCtl, 'exists',
                              new_callable=mock.PropertyMock) as mock_exists:
            mock_exists.return_value = True
            mock_status.return_value = (1, 'FakeStatus')
            self.assertFalse(self.ermine_ctl.ready)

    def test_ready_returns_true(self):
        """Test |ready| return True if tool returns good status (rc = 0)."""
        with mock.patch.object(
                BaseErmineCtl, 'status',
                new_callable=mock.PropertyMock) as mock_status, \
            mock.patch.object(BaseErmineCtl, 'exists',
                              new_callable=mock.PropertyMock) as mock_exists:
            mock_exists.return_value = True
            mock_status.return_value = (0, 'FakeStatus')
            self.assertTrue(self.ermine_ctl.ready)

    def test_status_raises_assertion_error_if_dne(self):
        """Test |status| returns |InvalidState| if tool does not exist."""
        with mock.patch.object(BaseErmineCtl,
                               'exists',
                               new_callable=mock.PropertyMock) as mock_exists:
            mock_exists.return_value = False

            self.assertRaises(AssertionError, getattr, self.ermine_ctl,
                              'status')

    def test_status_returns_rc_and_stdout(self):
        """Test |status| returns subprocess stdout and rc if tool exists."""
        with mock.patch.object(BaseErmineCtl,
                               'exists',
                               new_callable=mock.PropertyMock) as _:
            self._set_mock_proc(return_value=10)

            self.assertEqual(self.ermine_ctl.status, (10, 'foo'))

    def test_status_returns_timeout_state(self):
        """Test |status| returns |Timeout| if exception is raised."""
        with mock.patch.object(
                BaseErmineCtl, 'exists', new_callable=mock.PropertyMock) as _, \
                        mock.patch.object(logging, 'warning') as _:
            mock_proc = self._set_mock_proc(return_value=0)
            mock_proc.wait.side_effect = subprocess.TimeoutExpired(
                'cmd', 'some timeout')

            self.assertEqual(self.ermine_ctl.status, (-1, 'Timeout'))

    def test_wait_until_ready_raises_assertion_error_if_tool_dne(self):
        """Test |wait_until_ready| is returns false if tool does not exist."""
        with mock.patch.object(BaseErmineCtl,
                               'exists',
                               new_callable=mock.PropertyMock) as mock_exists:
            mock_exists.return_value = False

            self.assertRaises(AssertionError, self.ermine_ctl.wait_until_ready)

    def test_wait_until_ready_loops_until_ready(self):
        """Test |wait_until_ready| loops until |ready| returns True."""
        with mock.patch.object(BaseErmineCtl, 'exists',
                               new_callable=mock.PropertyMock) as mock_exists, \
                mock.patch.object(time, 'sleep') as mock_sleep, \
                mock.patch.object(BaseErmineCtl, 'ready',
                                  new_callable=mock.PropertyMock) as mock_ready:
            mock_exists.return_value = True
            mock_ready.side_effect = [False, False, False, True]

            self.ermine_ctl.wait_until_ready()

            self.assertEqual(mock_ready.call_count, 4)
            self.assertEqual(mock_sleep.call_count, 3)

    def test_wait_until_ready_raises_assertion_error_if_attempts_exceeded(
            self):
        """Test |wait_until_ready| loops if |ready| is not True n attempts."""
        with mock.patch.object(BaseErmineCtl, 'exists',
                               new_callable=mock.PropertyMock) as mock_exists, \
                mock.patch.object(time, 'sleep') as mock_sleep, \
                mock.patch.object(BaseErmineCtl, 'ready',
                                  new_callable=mock.PropertyMock) as mock_ready:
            mock_exists.return_value = True
            mock_ready.side_effect = [False] * 15 + [True]

            self.assertRaises(TimeoutError, self.ermine_ctl.wait_until_ready)

            self.assertEqual(mock_ready.call_count, 10)
            self.assertEqual(mock_sleep.call_count, 10)

    def test_take_to_shell_raises_assertion_error_if_tool_dne(self):
        """Test |take_to_shell| throws AssertionError if not ready is False."""
        with mock.patch.object(BaseErmineCtl,
                               'exists',
                               new_callable=mock.PropertyMock) as mock_exists:
            mock_exists.return_value = False
            self.assertRaises(AssertionError, self.ermine_ctl.take_to_shell)

    def test_take_to_shell_exits_on_complete_state(self):
        """Test |take_to_shell| exits with no calls if in completed state."""
        with mock.patch.object(BaseErmineCtl,
                               'wait_until_ready') as mock_wait_ready, \
                mock.patch.object(
                        BaseErmineCtl, 'status',
                        new_callable=mock.PropertyMock) as mock_status:
            mock_proc = self._set_mock_proc(return_value=52)
            mock_wait_ready.return_value = True
            mock_status.return_value = (0, 'Shell')

            self.ermine_ctl.take_to_shell()

            self.assertEqual(mock_proc.call_count, 0)

    def test_take_to_shell_invalid_state_raises_not_implemented_error(self):
        """Test |take_to_shell| raises exception if invalid state is returned.
        """
        with mock.patch.object(BaseErmineCtl,
                               'wait_until_ready') as mock_wait_ready, \
                mock.patch.object(
                        BaseErmineCtl, 'status',
                        new_callable=mock.PropertyMock) as mock_status:
            mock_wait_ready.return_value = True
            mock_status.return_value = (0, 'SomeUnknownState')

            self.assertRaises(NotImplementedError,
                              self.ermine_ctl.take_to_shell)

    def test_take_to_shell_with_max_transitions_raises_runtime_error(self):
        """Test |take_to_shell| raises exception on too many transitions.

        |take_to_shell| attempts to transition from one state to another.
        After 5 attempts, if this does not end in the completed state, an
        Exception is thrown.
        """
        with mock.patch.object(BaseErmineCtl,
                               'wait_until_ready') as mock_wait_ready, \
                mock.patch.object(
                        BaseErmineCtl, 'status',
                        new_callable=mock.PropertyMock) as mock_status:
            mock_wait_ready.return_value = True
            # Returns too many state transitions before CompleteState.
            mock_status.side_effect = [(0, 'Unknown'),
                                       (0, 'KnownWithPassword'),
                                       (0, 'Unknown')] * 3 + [
                                           (0, 'CompleteState')
                                       ]
            self.assertRaises(RuntimeError, self.ermine_ctl.take_to_shell)

    def test_take_to_shell_executes_known_commands(self):
        """Test |take_to_shell| executes commands if necessary.

        Some states can only be transitioned between with specific commands.
        These are executed by |take_to_shell| until the final test |Shell| is
        reached.
        """
        with mock.patch.object(BaseErmineCtl,
                               'wait_until_ready') as mock_wait_ready, \
                mock.patch.object(
                        BaseErmineCtl, 'status',
                        new_callable=mock.PropertyMock) as mock_status:
            self._set_mock_proc(return_value=0)
            mock_wait_ready.return_value = True
            mock_status.side_effect = [(0, 'Unknown'), (0, 'SetPassword'),
                                       (0, 'Shell')]

            self.ermine_ctl.take_to_shell()

            self.assertEqual(self.ermine_ctl.execute_command_async.call_count,
                             2)
            self.ermine_ctl.execute_command_async.assert_has_calls([
                mock.call(['erminectl', 'oobe', 'skip']),
                mock.call().communicate(),
                mock.call([
                    'erminectl', 'oobe', 'set_password',
                    'workstation_test_password'
                ]),
                mock.call().communicate()
            ])


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing modification_waiter.py."""

import unittest
import unittest.mock as mock

from modification_waiter import ModificationWaiter


class ModificationWaiterTest(unittest.TestCase):
    """Test ModificationWaiter."""

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[0, 60])
    def test_timeout(self, time_patch, sleep_patch) -> None:
        """The behavior when timeout happens, it shouldn't call other functions
        in the case."""
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 2)
        self.assertEqual(sleep_patch.call_count, 0)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[0, 5])
    @mock.patch('modification_waiter.os.path.getmtime', side_effect=[0])
    def test_quiet(self, getmtime_patch, time_patch, sleep_patch) -> None:
        """The behavior when no modifications happen during the last 5 seconds.
        """
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 2)
        self.assertEqual(getmtime_patch.call_count, 1)
        self.assertEqual(sleep_patch.call_count, 0)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[5, 10])
    @mock.patch('modification_waiter.os.path.getmtime', side_effect=[0])
    def test_mod_before_now(self, getmtime_patch, time_patch,
                            sleep_patch) -> None:
        """The behavior when no modifications happen before current time.
        """
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 2)
        self.assertEqual(getmtime_patch.call_count, 1)
        self.assertEqual(sleep_patch.call_count, 0)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[0, 5, 10])
    @mock.patch('modification_waiter.os.path.getmtime', side_effect=[5, 5])
    def test_mod_after_now(self, getmtime_patch, time_patch,
                           sleep_patch) -> None:
        """The behavior when a modification happens after current time.
        """
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 3)
        self.assertEqual(getmtime_patch.call_count, 2)
        self.assertEqual(sleep_patch.call_count, 1)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[0, 5, 10, 15])
    @mock.patch('modification_waiter.os.path.getmtime',
                side_effect=[5, 10, 10])
    def test_mod_twice_after_now(self, getmtime_patch, time_patch,
                                 sleep_patch) -> None:
        """The behavior when a modification happens after current time.
        """
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 4)
        self.assertEqual(getmtime_patch.call_count, 3)
        self.assertEqual(sleep_patch.call_count, 2)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[10, 5, 10, 15])
    @mock.patch('modification_waiter.os.path.getmtime',
                side_effect=[5, 10, 10])
    def test_decreased_time(self, getmtime_patch, time_patch,
                            sleep_patch) -> None:
        """The behavior when time.time() returns decreased values."""
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 4)
        self.assertEqual(getmtime_patch.call_count, 3)
        self.assertEqual(sleep_patch.call_count, 2)

    @mock.patch('modification_waiter.time.sleep')
    @mock.patch('modification_waiter.time.time', side_effect=[0, 5, 10, 15])
    @mock.patch('modification_waiter.os.path.getmtime', side_effect=[5, 10, 5])
    def test_decreased_mtime(self, getmtime_patch, time_patch,
                             sleep_patch) -> None:
        """The behavior when path.getmtime returns decreased values."""
        ModificationWaiter('/').__exit__(None, None, None)
        self.assertEqual(time_patch.call_count, 4)
        self.assertEqual(getmtime_patch.call_count, 3)
        self.assertEqual(sleep_patch.call_count, 2)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for tombstones."""

import unittest
from unittest import mock

# Imported before devil so devil_chromium initializes sys.path.
import tombstones

from devil.android import device_errors


class TombstonesTest(unittest.TestCase):
    def test_ClearAllTombstones_deviceUnreachable(self):
        device = mock.MagicMock()
        device.PathExists.side_effect = device_errors.DeviceUnreachableError(
            'device offline'
        )
        with self.assertRaises(device_errors.DeviceUnreachableError):
            tombstones.ClearAllTombstones(device)

    def test_ResolveTombstones_deviceUnreachable(self):
        device = mock.MagicMock()
        device.PathExists.side_effect = device_errors.DeviceUnreachableError(
            'device offline'
        )
        resolved = tombstones.ResolveTombstones(
            device,
            resolve_all_tombstones=False,
            include_stack_symbols=False,
            wipe_tombstones=False,
            tombstone_symbolizer=mock.MagicMock(),
        )
        self.assertEqual([], resolved)


if __name__ == '__main__':
    # Suppress logging messages.
    unittest.main(buffer=True)

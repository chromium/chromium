#!/usr/bin/env vpython3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing monitors.py."""

import importlib
import os
import tempfile
import unittest
import unittest.mock as mock

import monitors


def dump() -> bool:
    """Tries to dump the metrics into a temporary file and returns if the
    file exits."""
    with tempfile.TemporaryDirectory() as tmpdir:
        monitors.dump(tmpdir)
        return os.path.isfile(
            os.path.join(tmpdir, 'test_script_metrics.jsonpb'))


class MonitorsRealTest(unittest.TestCase):
    """Test real implementation of monitors.py."""

    def test_run_real_implementation(self) -> None:
        """Ensures the real version of the monitors is loaded."""
        importlib.reload(monitors)
        ave = monitors.average('test', 'run', 'real', 'implementation')
        ave.record(1)
        ave.record(2)
        self.assertTrue(dump())

    @mock.patch('os.path.isdir', side_effect=[False, True])
    def test_run_dummy_implementation(self, *_) -> None:
        """Ensures the dummy version of the monitors is loaded."""
        importlib.reload(monitors)
        ave = monitors.average('test', 'run', 'dummy', 'implementation')
        ave.record(1)
        ave.record(2)
        self.assertFalse(dump())

    @mock.patch('os.path.isdir', side_effect=[False, True])
    def test_with_dummy_implementation(self, *_) -> None:
        """Ensures the dummy version of the monitors can be used by 'with'
        statement."""
        importlib.reload(monitors)
        executed = False
        with monitors.time_consumption('test', 'with', 'dummy'):
            executed = True
        self.assertTrue(executed)
        self.assertFalse(dump())


if __name__ == '__main__':
    unittest.main()

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


class MonitorsRealTest(unittest.TestCase):
    """Test real implementation of monitors.py."""

    def test_run_real_implementation(self) -> None:
        """Ensure the real version of the monitors is loaded."""
        importlib.reload(monitors)
        ave = monitors.average('test', 'run', 'real', 'implementation')
        ave.record(1)
        ave.record(2)
        with tempfile.TemporaryDirectory() as tmpdir:
            monitors.dump(tmpdir)
            self.assertTrue(
                os.path.isfile(
                    os.path.join(tmpdir, 'test_script_metrics.jsonpb')))

    @mock.patch('os.path.isdir', side_effect=[False, True])
    def test_run_dummy_implementation(self, *_) -> None:
        """Ensure the dummy version of the monitors is loaded."""
        importlib.reload(monitors)
        ave = monitors.average('test', 'run', 'real', 'implementation')
        ave.record(1)
        ave.record(2)
        with tempfile.TemporaryDirectory() as tmpdir:
            monitors.dump(tmpdir)
            self.assertFalse(
                os.path.isfile(
                    os.path.join(tmpdir, 'test_script_metrics.jsonpb')))


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for tempfile_ext."""

import os
import pathlib
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

import tempfile_ext


class MkstempClosedUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the tempfile_ext.mkstemp_closed function."""

    def setUp(self):
        self.setUpPyfakefs()

    def test_default_behavior(self):
        """Tests that mkstemp_closed() works as expected."""
        file_path = None
        with tempfile_ext.mkstemp_closed() as f:
            self.assertIsInstance(f, pathlib.Path)
            self.assertTrue(os.path.exists(f))
            file_path = f
        self.assertIsNotNone(file_path)
        self.assertFalse(os.path.exists(file_path))

    def test_args_forwarded(self):
        """Tests that mkstemp_closed() works with arguments."""
        with tempfile.TemporaryDirectory() as temp_dir:
            file_path = None
            with tempfile_ext.mkstemp_closed(suffix='.txt',
                                             prefix='test_',
                                             directory=temp_dir) as f:
                self.assertIsInstance(f, pathlib.Path)
                self.assertTrue(os.path.exists(f))
                self.assertEqual(f.suffix, '.txt')
                self.assertTrue(f.name.startswith('test_'))
                self.assertEqual(f.parent, pathlib.Path(temp_dir))
                file_path = f
            self.assertIsNotNone(file_path)
            self.assertFalse(os.path.exists(file_path))


if __name__ == '__main__':
    unittest.main()

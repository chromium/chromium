#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import sys
import tempfile
import unittest
from unittest import mock

import clobber


class TestDelete(unittest.TestCase):
  def setUp(self):
    self.build_dir = tempfile.mkdtemp()

    pathlib.Path(os.path.join(self.build_dir, 'build.ninja')).touch()
    pathlib.Path(os.path.join(self.build_dir, 'build.ninja.d')).touch()

  def tearDown(self):
    shutil.rmtree(self.build_dir)

  def test_delete_build_dir_full(self):
    # Create a dummy file in the build dir and ensure it gets removed.
    dummy_file = os.path.join(self.build_dir, 'dummy')
    pathlib.Path(dummy_file).touch()

    clobber.delete_build_dir(self.build_dir)

    self.assertFalse(os.path.exists(dummy_file))

  def test_delete_build_dir_fail(self):
    # Make delete_dir() throw to ensure it's handled gracefully.

    with mock.patch('clobber.delete_dir', side_effect=OSError):
      with self.assertRaises(OSError):
        clobber.delete_build_dir(self.build_dir)


if __name__ == '__main__':
  unittest.main()

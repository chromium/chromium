#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import tempfile
import unittest

import clobber


class TestClean(unittest.TestCase):
  def setUp(self):
    self.build_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.build_dir)

  def test_gn_clean_ok(self):
    pathlib.Path(os.path.join(self.build_dir, 'build.ninja')).touch()
    pathlib.Path(os.path.join(self.build_dir, 'build.ninja.d')).touch()

    # Create a dummy file in the build dir and ensure it gets removed.
    dummy_file = os.path.join(self.build_dir, 'dummy')
    pathlib.Path(dummy_file).touch()

    clobber._clean_build_dir(self.build_dir)
    self.assertFalse(os.path.exists(dummy_file))

  def test_gn_clean_fail(self):
    # gn clean fails without build.ninja.
    # clean_build_dir() regenerates build.ninja internally.
    clobber._clean_build_dir(self.build_dir)


if __name__ == '__main__':
  unittest.main()

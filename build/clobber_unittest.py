#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import sys
import tempfile
import textwrap
import unittest
from unittest import mock

import clobber


class TestExtractBuildCommand(unittest.TestCase):
  def setUp(self):
    self.build_ninja_file, self.build_ninja_path = tempfile.mkstemp(text=True)

  def tearDown(self):
    os.close(self.build_ninja_file)
    os.remove(self.build_ninja_path)

  def test_normal_extraction(self):
    build_ninja_file_contents = textwrap.dedent("""
        ninja_required_version = 1.7.2

        rule gn
          command = ../../buildtools/gn --root=../.. -q --regeneration gen .
          pool = console
          description = Regenerating ninja files

        build build.ninja.stamp: gn
          generator = 1
          depfile = build.ninja.d

        build build.ninja: phony build.ninja.stamp
          generator = 1

        pool build_toolchain_action_pool
          depth = 72

        pool build_toolchain_link_pool
          depth = 23

        subninja toolchain.ninja
        subninja clang_newlib_x64/toolchain.ninja
        subninja glibc_x64/toolchain.ninja
        subninja irt_x64/toolchain.ninja
        subninja nacl_bootstrap_x64/toolchain.ninja
        subninja newlib_pnacl/toolchain.ninja

        build blink_python_tests: phony obj/blink_python_tests.stamp
        build blink_tests: phony obj/blink_tests.stamp

        default all
    """)  # Based off of a standard linux build dir.
    with open(self.build_ninja_path, 'w') as f:
      f.write(build_ninja_file_contents)

    expected_build_ninja_file_contents = textwrap.dedent("""
        ninja_required_version = 1.7.2

        rule gn
          command = ../../buildtools/gn --root=../.. -q --regeneration gen .
          pool = console
          description = Regenerating ninja files

        build build.ninja.stamp: gn
          generator = 1
          depfile = build.ninja.d

        build build.ninja: phony build.ninja.stamp
          generator = 1

    """)

    self.assertEqual(clobber.extract_gn_build_commands(self.build_ninja_path),
                     expected_build_ninja_file_contents)

  def test_unexpected_format(self):
    # No "build build.ninja:" line should make it return an empty string.
    build_ninja_file_contents = textwrap.dedent("""
        ninja_required_version = 1.7.2

        rule gn
          command = ../../buildtools/gn --root=../.. -q --regeneration gen .
          pool = console
          description = Regenerating ninja files

        subninja toolchain.ninja

        build blink_python_tests: phony obj/blink_python_tests.stamp
        build blink_tests: phony obj/blink_tests.stamp

    """)
    with open(self.build_ninja_path, 'w') as f:
      f.write(build_ninja_file_contents)

    self.assertEqual(clobber.extract_gn_build_commands(self.build_ninja_path),
                     '')


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

    with mock.patch('clobber._clean_dir', side_effect=OSError):
      with self.assertRaises(OSError):
        clobber.delete_build_dir(self.build_dir)

  @unittest.skipIf(sys.platform == 'win32', 'Symlinks are not allowed on Windows by default')
  def test_delete_build_dir_link(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      # create a symlink.
      build_dir = os.path.join(tmpdir, 'link')
      os.symlink(self.build_dir, build_dir)

      # create a dummy file.
      dummy_file = os.path.join(build_dir, 'dummy')
      pathlib.Path(dummy_file).touch()
      clobber.delete_build_dir(build_dir)

      self.assertFalse(os.path.exists(dummy_file))


if __name__ == '__main__':
  unittest.main()

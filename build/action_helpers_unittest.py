#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import shutil
import sys
import tempfile
import time
import unittest

import action_helpers


class ActionHelperTest(unittest.TestCase):
  def setUp(self):
    self.build_ninja_file, self.build_ninja_path = tempfile.mkstemp(text=True)

  def tearDown(self):
    os.close(self.build_ninja_file)
    os.remove(self.build_ninja_path)

  def test_atomic_output(self):
    with tempfile.NamedTemporaryFile('r+t') as f:
      f.write('test')
      f.flush()

      # Test that same contents does not change mtime.
      orig_mtime = os.path.getmtime(f.name)
      with action_helpers.atomic_output(f.name, 'wt') as af:
        time.sleep(.01)
        af.write('test')

      self.assertEqual(os.path.getmtime(f.name), orig_mtime)

      # Test that contents is written.
      with action_helpers.atomic_output(f.name, 'wt') as af:
        af.write('test2')
      self.assertEqual(pathlib.Path(f.name).read_text(), 'test2')
      self.assertNotEqual(os.path.getmtime(f.name), orig_mtime)

  def test_parse_gn_list(self):
    def test(value, expected):
      self.assertEqual(action_helpers.parse_gn_list(value), expected)

    test(None, [])
    test('', [])
    test('asdf', ['asdf'])
    test('["one"]', ['one'])
    test(['["one"]', '["two"]'], ['one', 'two'])
    test(['["one", "two"]', '["three"]'], ['one', 'two', 'three'])

  def test_write_depfile(self):
    with tempfile.NamedTemporaryFile('r+t') as f:

      def capture_output(inputs):
        action_helpers.write_depfile(f.name, 'output', inputs)
        f.seek(0)
        return f.read()

      self.assertEqual(capture_output(None), 'output: \n')
      self.assertEqual(capture_output([]), 'output: \n')
      self.assertEqual(capture_output(['a']), 'output: \\\n a\n')
      self.assertEqual(capture_output(['a', 'b']), 'output: \\\n a \\\n b\n')

      # Arg should be a list.
      with self.assertRaises(AssertionError):
        capture_output('a')

      # Do not use depfile itself as an output.
      with self.assertRaises(AssertionError):
        capture_output([f.name])

      # Do not use absolute paths.
      with self.assertRaises(AssertionError):
        capture_output(['/foo'])


if __name__ == '__main__':
  unittest.main()

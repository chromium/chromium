#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unpack_pak
import tempfile
import os
import shutil
import unittest

_HERE_DIR = os.path.dirname(__file__)

class UnpackPakTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._tmp_dirs = []

  def tearDown(self):
    for tmp_dir in self._tmp_dirs:
      shutil.rmtree(tmp_dir)

  def _setup_output_dirs(self):
    tmp_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self._tmp_dirs.append(tmp_dir)
    self._root_output_dir = os.path.join(tmp_dir, 'gen', 'mywebui')
    os.makedirs(self._root_output_dir)
    self._unpak_dir = os.path.normpath(
        os.path.join(self._root_output_dir, 'unpak')).replace('\\', '/')

  def _read_unpacked_file(self, file_name):
    assert self._unpak_dir
    return open(os.path.join(self._unpak_dir, file_name), 'r').read()

  def testMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH}'))

  def testGzippedMapFileLine(self):
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, false}'))
    self.assertTrue(unpack_pak.ParseLine('  {"path.js", IDR_PATH, true}'))

  def testUnpackResource(self):
    self._setup_output_dirs()
    unpack_pak.UnpackResource(
        os.path.join('gen', 'mywebui'), self._unpak_dir, [],
        'sub_dir/some_element.js', 'alert(\'hello from element in sub_dir\');')
    unpacked_contents = self._read_unpacked_file('sub_dir/some_element.js')
    self.assertIn('hello from element in sub_dir', unpacked_contents)

  def testUnpackGeneratedResource(self):
    self._setup_output_dirs()
    generated_resource_path = os.path.join(
        '@out_folder@', 'gen', 'mywebui', 'sub_dir', 'some_element.js')
    unpack_pak.UnpackResource(
        os.path.join('gen', 'mywebui'), self._unpak_dir,
        [], generated_resource_path,
        'alert(\'hello from element in sub_dir\');')
    unpacked_contents = self._read_unpacked_file('sub_dir/some_element.js')
    self.assertIn('hello from element in sub_dir', unpacked_contents)

  def testUnpackExcludedResource(self):
    self._setup_output_dirs()
    generated_shared_resource_path = os.path.join(
        '@out_folder@', 'gen', 'shared', 'shared_element.js')
    self.assertEqual(0, len(os.listdir(self._root_output_dir)))
    unpack_pak.UnpackResource(
        os.path.join('gen', 'mywebui'), self._unpak_dir,
        ['../shared/shared_element.js'], generated_shared_resource_path,
        'alert(\'hello from shared element\');')
    self.assertEqual(0, len(os.listdir(self._root_output_dir)))

if __name__ == '__main__':
  unittest.main()

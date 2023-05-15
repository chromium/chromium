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
import zipfile

import zip_helpers


def _make_test_zips(tmp_dir, create_conflct=False):
  zip1 = os.path.join(tmp_dir, 'A.zip')
  zip2 = os.path.join(tmp_dir, 'B.zip')
  with zipfile.ZipFile(zip1, 'w') as z:
    z.writestr('file1', 'AAAAA')
    z.writestr('file2', 'BBBBB')
  with zipfile.ZipFile(zip2, 'w') as z:
    z.writestr('file2', 'ABABA' if create_conflct else 'BBBBB')
    z.writestr('file3', 'CCCCC')
  return zip1, zip2


class ZipHelpersTest(unittest.TestCase):
  def test_merge_zips__identical_file(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      zip1, zip2 = _make_test_zips(tmp_dir)

      merged_zip = os.path.join(tmp_dir, 'merged.zip')
      zip_helpers.merge_zips(merged_zip, [zip1, zip2])

      with zipfile.ZipFile(merged_zip) as z:
        self.assertEqual(z.namelist(), ['file1', 'file2', 'file3'])

  def test_merge_zips__conflict(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      zip1, zip2 = _make_test_zips(tmp_dir, create_conflct=True)

      merged_zip = os.path.join(tmp_dir, 'merged.zip')
      with self.assertRaises(Exception):
        zip_helpers.merge_zips(merged_zip, [zip1, zip2])

  def test_merge_zips__conflict_with_append(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      zip1, zip2 = _make_test_zips(tmp_dir, create_conflct=True)

      with self.assertRaises(Exception):
        with zipfile.ZipFile(zip1, 'a') as dst_zip:
          zip_helpers.merge_zips(dst_zip, [zip2])


if __name__ == '__main__':
  unittest.main()

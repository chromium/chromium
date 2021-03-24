#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import math
import os
import shutil
import subprocess
import tempfile
import time
import unittest

import binary_sizes

from common import DIR_SOURCE_ROOT


class TestBinarySizes(unittest.TestCase):
  tmpdir = None

  @classmethod
  def setUpClass(cls):
    cls.tmpdir = tempfile.mkdtemp()

  @classmethod
  def tearDownClass(cls):
    shutil.rmtree(cls.tmpdir)

  # TODO(crbug.com/1145648): Add tests covering FAR file input and histogram
  # output.

  def testCommitFromBuildProperty(self):
    commit_position = binary_sizes.CommitPositionFromBuildProperty(
        'refs/heads/master@{#819458}')
    self.assertEqual(commit_position, 819458)

  def testCompressedSize(self):
    """Verifies that the compressed file size can be extracted from the
    blobfs-compression output."""

    uncompressed_file = tempfile.NamedTemporaryFile(delete=False)
    for line in range(200):
      uncompressed_file.write(
          'Lorem ipsum dolor sit amet, consectetur adipiscing elit. '
          'Sed eleifend')
    uncompressed_file.close()
    compressed_path = uncompressed_file.name + '.compressed'
    compressor_path = os.path.join(DIR_SOURCE_ROOT, 'third_party',
                                   'fuchsia-sdk', 'sdk', 'tools', 'x64',
                                   'blobfs-compression')
    subprocess.call([compressor_path, uncompressed_file.name, compressed_path])
    self.assertEqual(binary_sizes.CompressedSize(uncompressed_file.name),
                     os.path.getsize(compressed_path))
    os.remove(uncompressed_file.name)
    os.remove(compressed_path)


if __name__ == '__main__':
  unittest.main()

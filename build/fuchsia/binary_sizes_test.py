#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest

import binary_sizes


_EXAMPLE_BLOBS = """
{
  "web_engine": [
    {
      "merkle": "77e876447dd2daaaab7048d646e87fe8b6d9fecef6cbfcc4af30b8fbfa50b881",
      "path": "locales/ta.pak",
      "bytes": 17916,
      "is_counted": true,
      "size": 16384
    },
    {
      "merkle": "5f1932b8c9fe954f3c3fdb34ab2089d2af34e5a0cef90cad41a1cd37d92234bf",
      "path": "lib/libEGL.so",
      "bytes": 226960,
      "is_counted": true,
      "size": 90112
    },
    {
      "merkle": "9822fc0dd95cdd1cc46b5c6632a928a6ad19b76ed0157397d82a2f908946fc34",
      "path": "meta.far",
      "bytes": 24576,
      "is_counted": true,
      "size": 16384
    },
    {
      "merkle": "090aed4593c4f7d04a3ad80e9971c0532dd5b1d2bdf4754202cde510a88fd220",
      "path": "locales/ru.pak",
      "bytes": 11903,
      "is_counted": true,
      "size": 16384
    }
  ]
}
"""


class TestBinarySizes(unittest.TestCase):
  tmpdir = None

  @classmethod
  def setUpClass(cls):
    cls.tmpdir = tempfile.mkdtemp()

  @classmethod
  def tearDownClass(cls):
    shutil.rmtree(cls.tmpdir)


  def testReadAndWritePackageBlobs(self):
    # TODO(crbug.com/40219667): Disabled on Windows because Windows doesn't allow opening a
    # NamedTemporaryFile by name.
    if os.name == 'nt':
      return
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write(_EXAMPLE_BLOBS)
      tmp_file.flush()

      package_blobs = binary_sizes.ReadPackageBlobsJson(tmp_file.name)

    tmp_package_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
    tmp_package_file.close()
    try:
      binary_sizes.WritePackageBlobsJson(tmp_package_file.name, package_blobs)

      self.assertEqual(binary_sizes.ReadPackageBlobsJson(tmp_package_file.name),
                       package_blobs)
    finally:
      os.remove(tmp_package_file.name)

  def testReadAndWritePackageSizes(self):
    # TODO(crbug.com/40219667): Disabled on Windows because Windows doesn't allow opening a
    # NamedTemporaryFile by name.
    if os.name == 'nt':
      return
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write(_EXAMPLE_BLOBS)
      tmp_file.flush()
      blobs = binary_sizes.ReadPackageBlobsJson(tmp_file.name)

    sizes = binary_sizes.GetPackageSizes(blobs)

    new_sizes = {}
    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      binary_sizes.WritePackageSizesJson(tmp_file.name, sizes)
      new_sizes = binary_sizes.ReadPackageSizesJson(tmp_file.name)
      self.assertEqual(new_sizes, sizes)
      self.assertIn('web_engine', new_sizes)

  def testGetPackageSizesUsesBlobMerklesForCount(self):
    # TODO(crbug.com/40219667): Disabled on Windows because Windows doesn't allow opening a
    # NamedTemporaryFile by name.
    if os.name == 'nt':
      return
    blobs = json.loads(_EXAMPLE_BLOBS)

    # Make a duplicate of the last blob.
    last_blob = dict(blobs['web_engine'][-1])
    blobs['cast_runner'] = []
    last_blob['path'] = 'foo'  # Give a non-sense name, but keep merkle.

    # If the merkle is the same, the blob_count increases by 1.
    # This effectively reduces the size of the blobs size by half.
    # In both packages, despite it appearing in both and under different
    # names.
    blobs['cast_runner'].append(last_blob)

    with tempfile.NamedTemporaryFile(mode='w') as tmp_file:
      tmp_file.write(json.dumps(blobs))
      tmp_file.flush()
      blobs = binary_sizes.ReadPackageBlobsJson(tmp_file.name)

    sizes = binary_sizes.GetPackageSizes(blobs)

    self.assertEqual(sizes['cast_runner'].compressed, last_blob['size'] / 2)


if __name__ == '__main__':
  unittest.main()

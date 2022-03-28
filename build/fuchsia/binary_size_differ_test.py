#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import tempfile
import unittest

import binary_size_differ
import binary_sizes

from common import DIR_SOURCE_ROOT

_EXAMPLE_BLOBS_BEFORE = """
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
      "is_counted": false,
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


class BinarySizeDifferTest(unittest.TestCase):
  def ChangeBlobSize(self, blobs, package, name, increase):
    original_blob = blobs[package][name]
    new_blob = binary_sizes.Blob(name=original_blob.name,
                                 hash=original_blob.hash,
                                 uncompressed=original_blob.uncompressed,
                                 compressed=original_blob.compressed + increase,
                                 is_counted=original_blob.is_counted)
    blobs[package][name] = new_blob

  def testComputePackageDiffs(self):
    # TODO(1309977): Disabled on Windows because Windows doesn't allow opening a
    # NamedTemporaryFile by name.
    if os.name == 'nt':
      return
    with tempfile.NamedTemporaryFile(mode='w') as before_file:
      before_file.write(_EXAMPLE_BLOBS_BEFORE)
      before_file.flush()
      blobs = binary_sizes.ReadPackageBlobsJson(before_file.name)

      # No change.
      growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                      before_file.name)
      self.assertEqual(growth['status_code'], 0)
      self.assertEqual(growth['compressed']['web_engine'], 0)

      after_file = tempfile.NamedTemporaryFile(mode='w', delete=True)
      after_file.close()
      try:
        # Increase a blob, but below the limit.
        other_blobs = copy.deepcopy(blobs)
        self.ChangeBlobSize(other_blobs, 'web_engine', 'locales/ru.pak',
                            8 * 1024)
        binary_sizes.WritePackageBlobsJson(after_file.name, other_blobs)

        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], 0)
        self.assertEqual(growth['compressed']['web_engine'], 8 * 1024)

        # Increase beyond the limit (adds another 8k)
        self.ChangeBlobSize(other_blobs, 'web_engine', 'locales/ru.pak',
                            8 * 1024 + 1)
        binary_sizes.WritePackageBlobsJson(after_file.name, other_blobs)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], 1)
        self.assertEqual(growth['compressed']['web_engine'], 16 * 1024 + 1)

        other_blobs = copy.deepcopy(blobs)
        # Increase the limit of multiple blobs.
        self.ChangeBlobSize(other_blobs, 'web_engine', 'locales/ru.pak',
                            (8 * 1024 + 1))
        self.ChangeBlobSize(other_blobs, 'web_engine', 'locales/ta.pak',
                            (8 * 1024))
        binary_sizes.WritePackageBlobsJson(after_file.name, other_blobs)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], 1)
        self.assertEqual(growth['compressed']['web_engine'], 16 * 1024 + 1)

        other_blobs = copy.deepcopy(blobs)
        # Increase the limit of is_counted=false does not increase limit.
        self.ChangeBlobSize(other_blobs, 'web_engine', 'meta.far', (16 * 1024))
        binary_sizes.WritePackageBlobsJson(after_file.name, other_blobs)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], 0)
        self.assertEqual(growth['compressed']['web_engine'], 0)

      finally:
        os.remove(after_file.name)


if __name__ == '__main__':
  unittest.main()

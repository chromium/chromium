#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import tempfile
from typing import MutableMapping, Optional
import unittest

import binary_size_differ
import binary_sizes

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
  def ChangePackageSize(
      self,
      packages: MutableMapping[str, binary_sizes.PackageSizes],
      name: str,
      compressed_increase: int,
      uncompressed_increase: Optional[int] = None):
    if uncompressed_increase is None:
      uncompressed_increase = compressed_increase
    original_package = packages[name]
    new_package = binary_sizes.PackageSizes(
        compressed=original_package.compressed + compressed_increase,
        uncompressed=original_package.uncompressed + uncompressed_increase)
    packages[name] = new_package

  def testComputePackageDiffs(self):
    # TODO(crbug.com/40219667): Disabled on Windows because Windows doesn't allow opening a
    # NamedTemporaryFile by name.
    if os.name == 'nt':
      return

    SUCCESS = 0
    FAILURE = 1
    ROLLER_SIZE_WARNING = 2
    with tempfile.NamedTemporaryFile(mode='w') as before_file:
      before_file.write(_EXAMPLE_BLOBS_BEFORE)
      before_file.flush()
      blobs = binary_sizes.ReadPackageBlobsJson(before_file.name)
      sizes = binary_sizes.GetPackageSizes(blobs)
      binary_sizes.WritePackageSizesJson(before_file.name, sizes)

      # No change.
      growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                      before_file.name)
      self.assertEqual(growth['status_code'], SUCCESS)
      self.assertEqual(growth['compressed']['web_engine'], 0)

      after_file = tempfile.NamedTemporaryFile(mode='w', delete=True)
      after_file.close()
      try:
        # Increase a blob, but below the limit.
        other_sizes = copy.deepcopy(sizes)
        self.ChangePackageSize(other_sizes, 'web_engine', 8 * 1024)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)

        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], SUCCESS)
        self.assertEqual(growth['compressed']['web_engine'], 8 * 1024)

        # Increase beyond the limit (adds another 8k)
        self.ChangePackageSize(other_sizes, 'web_engine', 8 * 1024 + 1)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], FAILURE)
        self.assertEqual(growth['compressed']['web_engine'], 16 * 1024 + 1)
        self.assertIn('check failed', growth['summary'])
        self.assertIn(f'web_engine (compressed) grew by {16 * 1024 + 1} bytes',
                      growth['summary'])

        # Increase beyond the limit, but compressed does not increase.
        binary_sizes.WritePackageSizesJson(before_file.name, other_sizes)
        self.ChangePackageSize(other_sizes,
                               'web_engine',
                               16 * 1024 + 1,
                               uncompressed_increase=0)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['uncompressed']['web_engine'], SUCCESS)
        self.assertEqual(growth['status_code'], SUCCESS)
        self.assertEqual(growth['compressed']['web_engine'], 16 * 1024 + 1)

        # Increase beyond the limit, but compressed goes down.
        binary_sizes.WritePackageSizesJson(before_file.name, other_sizes)
        self.ChangePackageSize(other_sizes,
                               'web_engine',
                               16 * 1024 + 1,
                               uncompressed_increase=-4 * 1024)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], SUCCESS)
        self.assertEqual(growth['compressed']['web_engine'], 16 * 1024 + 1)

        # Increase beyond the second limit. Fails, regardless of uncompressed.
        binary_sizes.WritePackageSizesJson(before_file.name, other_sizes)
        self.ChangePackageSize(other_sizes,
                               'web_engine',
                               100 * 1024 + 1,
                               uncompressed_increase=-4 * 1024)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name)
        self.assertEqual(growth['status_code'], FAILURE)
        self.assertEqual(growth['compressed']['web_engine'], 100 * 1024 + 1)

        # Increase beyond the second limit, but roller authored CL.
        binary_sizes.WritePackageSizesJson(before_file.name, other_sizes)
        self.ChangePackageSize(other_sizes,
                               'web_engine',
                               100 * 1024 + 1,
                               uncompressed_increase=-4 * 1024)
        binary_sizes.WritePackageSizesJson(after_file.name, other_sizes)
        growth = binary_size_differ.ComputePackageDiffs(before_file.name,
                                                        after_file.name,
                                                        author='big-autoroller')
        self.assertEqual(growth['status_code'], ROLLER_SIZE_WARNING)
        self.assertEqual(growth['compressed']['web_engine'], 100 * 1024 + 1)
        self.assertNotIn('check failed', growth['summary'])
        self.assertIn('growth by an autoroller will be ignored',
                      growth['summary'])
        self.assertIn(f'web_engine (compressed) grew by {100 * 1024 + 1} bytes',
                      growth['summary'])
      finally:
        os.remove(after_file.name)


if __name__ == '__main__':
  unittest.main()

#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing common.py."""

import os
import tempfile
import unittest
import unittest.mock as mock

import common


@unittest.skipIf(os.name == 'nt', 'Fuchsia tests not supported on Windows')
class CommonTest(unittest.TestCase):
    """Test common.py methods."""
    def test_find_in_dir_returns_file_or_dir_if_searching(self) -> None:
        """Test |find_in_dir| returns files if searching for file, or None."""
        # Make the directory structure.
        with tempfile.TemporaryDirectory() as tmp_dir:
            with tempfile.NamedTemporaryFile(dir=tmp_dir) as tmp_file, \
                tempfile.TemporaryDirectory(dir=tmp_dir) as inner_tmp_dir:

                # Structure is now:
                # temp_dir/
                # temp_dir/inner_dir1
                # temp_dir/tempfile1
                # File is not a dir, so returns None.
                self.assertIsNone(
                    common.find_in_dir(os.path.basename(tmp_file.name),
                                       parent_dir=tmp_dir))

                # Repeat for directory.
                self.assertEqual(
                    common.find_in_dir(inner_tmp_dir, parent_dir=tmp_dir),
                    inner_tmp_dir)

    def test_find_image_in_sdk_searches_images_in_product_bundle(self):
        """Test |find_image_in_sdk| searches for 'images' if product-bundle."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            os.makedirs(os.path.join(tmp_dir, 'sdk'), exist_ok=True)
            os.makedirs(os.path.join(tmp_dir, 'images', 'workstation-product',
                                     'images'),
                        exist_ok=True)
            with mock.patch('common.SDK_ROOT', os.path.join(tmp_dir, 'sdk')):
                self.assertEqual(
                    common.find_image_in_sdk('workstation-product'),
                    os.path.join(tmp_dir, 'images', 'workstation-product',
                                 'images'))


if __name__ == '__main__':
    unittest.main()

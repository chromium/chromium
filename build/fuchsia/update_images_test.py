#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from parameterized import parameterized

from update_images import _GetImageOverrideInfo
from update_images import GetImageLocationInfo


@mock.patch('update_images.GetSDKOverrideGCSPath')
class TestGetImageOverrideInfo(unittest.TestCase):
  def testLocationIsNone(self, mock_sdk_loc):
    mock_sdk_loc.return_value = None

    actual = _GetImageOverrideInfo()
    self.assertIsNone(actual)

  def testBadLocationStr(self, mock_sdk_loc):
    mock_sdk_loc.return_value = 'bad-format-string'

    with self.assertRaises(Exception):
      _GetImageOverrideInfo()

  @parameterized.expand([
      ('gs://my-bucket/development/my-hash/sdk', {
          'bucket': 'my-bucket',
          'image_hash': 'my-hash'
      }),
      ('gs://my-bucket/development/my-hash', {
          'bucket': 'my-bucket',
          'image_hash': 'my-hash'
      }),
      ('gs://my-bucket/development/my-hash/', {
          'bucket': 'my-bucket',
          'image_hash': 'my-hash'
      }),
  ])
  def testValidLocation(self, mock_sdk_loc, in_path, expected):
    mock_sdk_loc.return_value = in_path

    actual = _GetImageOverrideInfo()
    self.assertEqual(actual, expected)


@mock.patch('update_images.GetImageHash')
@mock.patch('update_images.GetOverrideCloudStorageBucket')
@mock.patch('update_images._GetImageOverrideInfo')
class TestGetImageLocationInfo(unittest.TestCase):
  def testNoOverride(self, mock_image_override, mock_override_bucket,
                     mock_image_hash):
    mock_image_override.return_value = None
    mock_override_bucket.return_value = None
    mock_image_hash.return_value = 'image-hash'

    actual = GetImageLocationInfo('my-bucket')
    self.assertEqual(actual, {
        'bucket': 'my-bucket',
        'image_hash': 'image-hash',
    })

  def testOverride(self, mock_image_override, mock_override_bucket,
                   mock_image_hash):
    override_info = {
        'bucket': 'override-bucket',
        'image_hash': 'override-hash',
    }
    mock_image_override.return_value = override_info
    mock_override_bucket.return_value = None
    mock_image_hash.return_value = 'image-hash'

    actual = GetImageLocationInfo('my-bucket')
    self.assertEqual(actual, override_info)


if __name__ == '__main__':
  unittest.main()

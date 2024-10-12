#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from unittest.mock import patch, mock_open

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..')))
from pylib.local.emulator import avd
from pylib.local.emulator.proto import avd_pb2


def CreateAvdSettings():
  # python generated codes are simplified since Protobuf v3.20.0 and cause
  # pylint error: https://github.com/protocolbuffers/protobuf/issues/9730
  # pylint: disable=no-member
  return avd_pb2.AvdSettings()


class AvdCreateTest(unittest.TestCase):

  _CONFIG = """
  avd_settings {
    screen {
      density: 480
      height: 1920
      width: 1080
    }
  }
  """

  def setUp(self):
    with patch('builtins.open', mock_open(read_data=self._CONFIG)):
      self.avd_config = avd.AvdConfig('/path/to/creation.textpb')

  def testGetAvdSettingsWithoutVariants(self):
    avd_settings = self.avd_config.GetAvdSettings()
    self.assertEqual(avd_settings.screen.density, 480)
    self.assertEqual(avd_settings.screen.height, 1920)
    self.assertEqual(avd_settings.screen.width, 1080)

    with self.assertRaises(avd.AvdException):
      self.avd_config.GetAvdSettings('baz')

  def testGetAvdSettingsWithVariants(self):
    avd_settings = CreateAvdSettings()
    avd_settings.avd_properties['disk.dataPartition.size'] = '4G'
    self.avd_config.avd_variants['foo'].CopyFrom(avd_settings)
    avd_settings.avd_properties['disk.dataPartition.size'] = '8G'
    self.avd_config.avd_variants['bar'].CopyFrom(avd_settings)

    avd_settings_foo = self.avd_config.GetAvdSettings('foo')
    avd_settings_bar = self.avd_config.GetAvdSettings('bar')

    # The value of screen should be the same.
    self.assertEqual(avd_settings_foo.screen.density, 480)
    self.assertEqual(avd_settings_foo.screen.height, 1920)
    self.assertEqual(avd_settings_foo.screen.width, 1080)

    self.assertEqual(avd_settings_bar.screen.density, 480)
    self.assertEqual(avd_settings_bar.screen.height, 1920)
    self.assertEqual(avd_settings_bar.screen.width, 1080)

    # The values of the avd_properties should be different.
    self.assertEqual(avd_settings_foo.avd_properties['disk.dataPartition.size'],
                     '4G')
    self.assertEqual(avd_settings_bar.avd_properties['disk.dataPartition.size'],
                     '8G')

    # The base avd_settings should not be changed.
    self.assertEqual(self.avd_config.avd_settings.screen.density, 480)
    self.assertEqual(self.avd_config.avd_settings.screen.height, 1920)
    self.assertEqual(self.avd_config.avd_settings.screen.width, 1080)
    self.assertNotIn('disk.dataPartition.size',
                     self.avd_config.avd_settings.avd_properties)

    # Non-exist variant
    with self.assertRaises(avd.AvdException):
      self.avd_config.GetAvdSettings('baz')

  def testGetMetadataWithoutVariants(self):
    metadata = self.avd_config.GetMetadata()
    self.assertIn('avd_proto_path', metadata)
    self.assertIn('is_available', metadata)
    self.assertNotIn('avd_variants', metadata)

  def testGetMetadataWithVariants(self):
    avd_settings = CreateAvdSettings()
    avd_settings.avd_properties['disk.dataPartition.size'] = '4G'
    self.avd_config.avd_variants['foo'].CopyFrom(avd_settings)
    avd_settings.avd_properties['disk.dataPartition.size'] = '8G'
    self.avd_config.avd_variants['bar'].CopyFrom(avd_settings)

    metadata = self.avd_config.GetMetadata()
    self.assertIn('avd_proto_path', metadata)
    self.assertIn('is_available', metadata)
    self.assertIn('avd_variants', metadata)
    self.assertEqual(['bar', 'foo'], metadata['avd_variants'])


if __name__ == "__main__":
  unittest.main()

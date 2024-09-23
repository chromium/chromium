#! /usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from pylib import constants
from pylib.utils import device_dependencies


class DevicePathComponentsForTest(unittest.TestCase):

  def testCheckedInFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'foo', 'bar', 'baz.txt')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual([None, 'foo', 'bar', 'baz.txt'],
                     device_dependencies.DevicePathComponentsFor(
                         test_path, output_directory))

  def testOutputDirectoryFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'icudtl.dat')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual([None, 'icudtl.dat'],
                     device_dependencies.DevicePathComponentsFor(
                         test_path, output_directory))

  def testOutputDirectorySubdirFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'test_dir', 'icudtl.dat')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual([None, 'test_dir', 'icudtl.dat'],
                     device_dependencies.DevicePathComponentsFor(
                         test_path, output_directory))

  def testOutputDirectoryPakFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'foo.pak')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual([None, 'paks', 'foo.pak'],
                     device_dependencies.DevicePathComponentsFor(
                         test_path, output_directory))


class SubstituteDeviceRootTest(unittest.TestCase):

  def testNoneDevicePath(self):
    self.assertEqual(
        '/fake/device/root',
        device_dependencies.SubstituteDeviceRootSingle(None,
                                                       '/fake/device/root'))

  def testStringDevicePath(self):
    self.assertEqual(
        '/another/fake/device/path',
        device_dependencies.SubstituteDeviceRootSingle(
            '/another/fake/device/path', '/fake/device/root'))

  def testListWithNoneDevicePath(self):
    self.assertEqual(
        '/fake/device/root/subpath',
        device_dependencies.SubstituteDeviceRootSingle([None, 'subpath'],
                                                       '/fake/device/root'))

  def testListWithoutNoneDevicePath(self):
    self.assertEqual(
        '/another/fake/device/path',
        device_dependencies.SubstituteDeviceRootSingle(
            ['/', 'another', 'fake', 'device', 'path'], '/fake/device/root'))


if __name__ == '__main__':
  unittest.main()

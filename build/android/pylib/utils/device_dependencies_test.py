#! /usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for device_dependencies.py.

Example usage:
  vpython3 device_dependencies_test.py
"""

import os
import tempfile
import unittest
from unittest import mock

from pathlib import Path
import sys

build_android_path = Path(__file__).parents[2]
sys.path.append(str(build_android_path))

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


@mock.patch('pylib.constants.GetOutDirectory')
class GetDataDependenciesTest(unittest.TestCase):

  def testSimple(self, mock_get_out_dir):
    with tempfile.TemporaryDirectory() as out_dir:
      runtime_deps_file_path = os.path.join(out_dir, 'runtime_deps_file')
      mock_get_out_dir.return_value = out_dir
      with open(runtime_deps_file_path, 'w') as f:
        f.write('foo.pak\n')
        f.write('foo/bar.py\n')
        f.write('bin/run_some_test\n')
      deps = device_dependencies.GetDataDependencies(runtime_deps_file_path)
      self.assertEqual(1, len(deps))
      self.assertEqual([None, 'paks', 'foo.pak'], deps[0][1])

  def testWeirdBuildDirName(self, mock_get_out_dir):
    with tempfile.TemporaryDirectory(suffix='Android32_(more/') as out_dir:
      runtime_deps_file_path = os.path.join(out_dir, 'runtime_deps_file')
      mock_get_out_dir.return_value = out_dir
      with open(runtime_deps_file_path, 'w') as f:
        f.write('foo/bar.txt\n')
      deps = device_dependencies.GetDataDependencies(runtime_deps_file_path)
      self.assertEqual(1, len(deps))
      self.assertEqual([None, 'foo', 'bar.txt'], deps[0][1])


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

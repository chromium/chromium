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


class DevicePathForTest(unittest.TestCase):

  def testCheckedInFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'foo', 'bar', 'baz.txt')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual(
        'foo/bar/baz.txt',
        device_dependencies.DevicePathFor(test_path, output_directory))

  def testOutputDirectoryFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'icudtl.dat')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual(
        'icudtl.dat',
        device_dependencies.DevicePathFor(test_path, output_directory))

  def testOutputDirectorySubdirFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'test_dir', 'icudtl.dat')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual(
        'test_dir/icudtl.dat',
        device_dependencies.DevicePathFor(test_path, output_directory))

  def testOutputDirectoryPakFile(self):
    test_path = os.path.join(constants.DIR_SOURCE_ROOT, 'out-foo', 'Release',
                             'foo.pak')
    output_directory = os.path.join(
        constants.DIR_SOURCE_ROOT, 'out-foo', 'Release')
    self.assertEqual(
        'paks/foo.pak',
        device_dependencies.DevicePathFor(test_path, output_directory))


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
      self.assertEqual('paks/foo.pak', deps[0][1])

  def testWeirdBuildDirName(self, mock_get_out_dir):
    with tempfile.TemporaryDirectory(suffix='Android32_(more/') as out_dir:
      runtime_deps_file_path = os.path.join(out_dir, 'runtime_deps_file')
      mock_get_out_dir.return_value = out_dir
      with open(runtime_deps_file_path, 'w') as f:
        f.write('foo/bar.txt\n')
      deps = device_dependencies.GetDataDependencies(runtime_deps_file_path)
      self.assertEqual(1, len(deps))
      self.assertEqual('foo/bar.txt', deps[0][1])


class SubstituteDeviceRootTest(unittest.TestCase):

  def testNoneDevicePath(self):
    self.assertEqual(
        '/fake/device/root',
        device_dependencies.SubstituteDeviceRootSingle(None,
                                                       '/fake/device/root'))

  def testRelativeDevicePath(self):
    self.assertEqual(
        '/fake/device/root/foo/bar',
        device_dependencies.SubstituteDeviceRootSingle('foo/bar',
                                                       '/fake/device/root'))

  def testAbsoluteDevicePath(self):
    self.assertEqual(
        '/another/absolute/path',
        device_dependencies.SubstituteDeviceRootSingle('/another/absolute/path',
                                                       '/fake/device/root'))


class FilterDataDependenciesTest(unittest.TestCase):

  def testFilterDataDependencies(self):
    # Mock constants
    source_root = '/src'

    deps = [
        (
            '/src/chrome/test/data/android/file1.txt',
            'chrome/test/data/android/file1.txt',
        ),
        (
            '/src/chrome/test/data/android/subdir/file2.txt',
            'chrome/test/data/android/subdir/file2.txt',
        ),
        ('/src/out/Debug/other/path/file3.txt', 'other/path/file3.txt'),
        (
            '/src/net/data/ssl/certificates/root_ca_cert.pem',
            'net/data/ssl/certificates/root_ca_cert.pem',
        ),
        (
            '/src/net/data/ssl/certificates/other_cert.pem',
            'net/data/ssl/certificates/other_cert.pem',
        ),
    ]

    # Case 1: Allowlist approach (Only chrome/test/data/android/* is
    # allowlisted, ended with -*)
    filters = [
        '+//chrome/test/data/android/*',
        '-*',
    ]
    with mock.patch('pylib.constants.DIR_SOURCE_ROOT', source_root):
      filtered = device_dependencies.FilterDataDependencies(deps, filters)

    expected = [
        (
            '/src/chrome/test/data/android/file1.txt',
            'chrome/test/data/android/file1.txt',
        ),
        (
            '/src/chrome/test/data/android/subdir/file2.txt',
            'chrome/test/data/android/subdir/file2.txt',
        ),
    ]
    self.assertEqual(filtered, expected)

    # Case 2: Allowlist approach (Both chrome/test/data/android/* and
    # net/data/ssl/certificates/* are allowlisted, ended with -*)
    filters = [
        '+//chrome/test/data/android/*',
        '+//net/data/ssl/certificates/*',
        '-*',
    ]
    with mock.patch('pylib.constants.DIR_SOURCE_ROOT', source_root):
      filtered = device_dependencies.FilterDataDependencies(deps, filters)

    expected = [
        (
            '/src/chrome/test/data/android/file1.txt',
            'chrome/test/data/android/file1.txt',
        ),
        (
            '/src/chrome/test/data/android/subdir/file2.txt',
            'chrome/test/data/android/subdir/file2.txt',
        ),
        (
            '/src/net/data/ssl/certificates/root_ca_cert.pem',
            'net/data/ssl/certificates/root_ca_cert.pem',
        ),
        (
            '/src/net/data/ssl/certificates/other_cert.pem',
            'net/data/ssl/certificates/other_cert.pem',
        ),
    ]
    self.assertEqual(filtered, expected)

    # Case 3: Blocklist approach (Default keep, only
    # net/data/ssl/certificates/* is blocklisted)
    filters = [
        '-//net/data/ssl/certificates/*',
    ]
    with mock.patch('pylib.constants.DIR_SOURCE_ROOT', source_root):
      filtered = device_dependencies.FilterDataDependencies(deps, filters)

    expected = [
        (
            '/src/chrome/test/data/android/file1.txt',
            'chrome/test/data/android/file1.txt',
        ),
        (
            '/src/chrome/test/data/android/subdir/file2.txt',
            'chrome/test/data/android/subdir/file2.txt',
        ),
        ('/src/out/Debug/other/path/file3.txt', 'other/path/file3.txt'),
    ]
    self.assertEqual(filtered, expected)

    # Case 4: Specific blocklist before general allowlist (First match wins)
    # Remove root_ca_cert.pem, keep other certificates, remove everything else.
    filters = [
        '-//net/data/ssl/certificates/root_ca_cert.pem',
        '+//net/data/ssl/certificates/*',
        '-*',
    ]
    with mock.patch('pylib.constants.DIR_SOURCE_ROOT', source_root):
      filtered = device_dependencies.FilterDataDependencies(deps, filters)

    expected = [
        (
            '/src/net/data/ssl/certificates/other_cert.pem',
            'net/data/ssl/certificates/other_cert.pem',
        ),
    ]
    self.assertEqual(filtered, expected)

    # Case 5: Invalid filter (should raise ValueError)
    filters = [
        'invalid_filter',
    ]
    with mock.patch('pylib.constants.DIR_SOURCE_ROOT', source_root):
      with self.assertRaises(ValueError):
        device_dependencies.FilterDataDependencies(deps, filters)


if __name__ == '__main__':
  unittest.main()

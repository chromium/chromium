#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import list_class_verification_failures as list_verification

import devil_chromium  # pylint: disable=unused-import
from devil.android import device_errors
from devil.android import device_utils
from devil.android.ndk import abis
from devil.android.sdk import version_codes

import mock  # pylint: disable=import-error


def _CreateOdexLine(java_class_name, type_idx, verification_status):
  """Create a rough approximation of a line of oatdump output."""
  return ('{type_idx}: L{java_class}; (offset=0xac) (type_idx={type_idx}) '
          '({verification}) '
          '(OatClassNoneCompiled)'.format(type_idx=type_idx,
                                          java_class=java_class_name,
                                          verification=verification_status))


def _ClassForName(name, classes):
  return next(c for c in classes if c.name == name)


class _DetermineDeviceToUseTest(unittest.TestCase):

  def testDetermineDeviceToUse_emptyListWithOneAttachedDevice(self):
    fake_attached_devices = ['123']
    user_specified_devices = []
    device_utils.DeviceUtils.HealthyDevices = mock.MagicMock(
        return_value=fake_attached_devices)
    result = list_verification.DetermineDeviceToUse(user_specified_devices)
    self.assertEqual(result, fake_attached_devices[0])
    # pylint: disable=no-member
    device_utils.DeviceUtils.HealthyDevices.assert_called_with(device_arg=None)
    # pylint: enable=no-member

  def testDetermineDeviceToUse_emptyListWithNoAttachedDevices(self):
    user_specified_devices = []
    device_utils.DeviceUtils.HealthyDevices = mock.MagicMock(
        side_effect=device_errors.NoDevicesError())
    with self.assertRaises(device_errors.NoDevicesError) as _:
      list_verification.DetermineDeviceToUse(user_specified_devices)
    # pylint: disable=no-member
    device_utils.DeviceUtils.HealthyDevices.assert_called_with(device_arg=None)
    # pylint: enable=no-member

  def testDetermineDeviceToUse_oneElementListWithOneAttachedDevice(self):
    user_specified_devices = ['123']
    fake_attached_devices = ['123']
    device_utils.DeviceUtils.HealthyDevices = mock.MagicMock(
        return_value=fake_attached_devices)
    result = list_verification.DetermineDeviceToUse(user_specified_devices)
    self.assertEqual(result, fake_attached_devices[0])
    # pylint: disable=no-member
    device_utils.DeviceUtils.HealthyDevices.assert_called_with(
        device_arg=user_specified_devices)
    # pylint: enable=no-member


class _ListClassVerificationFailuresTest(unittest.TestCase):

  def testPathToDexForPlatformVersion_noPaths(self):
    sdk_int = version_codes.LOLLIPOP
    paths_to_apk = []
    package_name = 'package.name'
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)

    with self.assertRaises(list_verification.DeviceOSError) as cm:
      list_verification.FindOdexFiles(device, package_name)
    message = str(cm.exception)
    self.assertIn('Could not find data directory', message)

  def testPathToDexForPlatformVersion_multiplePaths(self):
    sdk_int = version_codes.LOLLIPOP
    paths_to_apk = ['/first/path', '/second/path']
    package_name = 'package.name'
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)

    odex_files = list_verification.FindOdexFiles(device, package_name)
    self.assertEqual(odex_files, [
        '/data/dalvik-cache/arm64/data@app@first@base.apk@classes.dex',
        '/data/dalvik-cache/arm64/data@app@second@base.apk@classes.dex'
    ])

  def testPathToDexForPlatformVersion_dalvikApiLevel(self):
    sdk_int = version_codes.KITKAT
    paths_to_apk = ['/some/path']
    package_name = 'package.name'
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)

    with self.assertRaises(list_verification.UnsupportedDeviceError) as _:
      list_verification.FindOdexFiles(device, package_name)

  def testPathToDexForPlatformVersion_lollipopArm(self):
    sdk_int = version_codes.LOLLIPOP
    package_name = 'package.name'
    paths_to_apk = ['/some/path/{}-1/base.apk'.format(package_name)]
    arch = 'arm'

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)
    device.FileExists = mock.MagicMock(return_value=True)

    odex_files = list_verification.FindOdexFiles(device, package_name)
    self.assertEqual(
        odex_files,
        ['/data/dalvik-cache/arm/data@app@package.name-1@base.apk@classes.dex'])

  def testPathToDexForPlatformVersion_mashmallowArm(self):
    sdk_int = version_codes.MARSHMALLOW
    package_name = 'package.name'
    paths_to_apk = ['/some/path/{}-1/base.apk'.format(package_name)]
    arch = 'arm'

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)
    device.FileExists = mock.MagicMock(return_value=True)

    odex_files = list_verification.FindOdexFiles(device, package_name)
    self.assertEqual(odex_files,
                     ['/some/path/package.name-1/oat/arm/base.odex'])

  def testPathToDexForPlatformVersion_mashmallowArm64(self):
    sdk_int = version_codes.MARSHMALLOW
    package_name = 'package.name'
    paths_to_apk = ['/some/path/{}-1/base.apk'.format(package_name)]
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)
    device.FileExists = mock.MagicMock(return_value=True)

    odex_files = list_verification.FindOdexFiles(device, package_name)
    self.assertEqual(odex_files,
                     ['/some/path/package.name-1/oat/arm64/base.odex'])

  def testPathToDexForPlatformVersion_pieNoOdexFile(self):
    sdk_int = version_codes.PIE
    package_name = 'package.name'
    paths_to_apk = ['/some/path/{}-1/base.apk'.format(package_name)]
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)
    device.FileExists = mock.MagicMock(return_value=False)

    with self.assertRaises(list_verification.DeviceOSError) as cm:
      list_verification.FindOdexFiles(device, package_name)
    message = str(cm.exception)
    self.assertIn('you must run dex2oat on debuggable apps on >= P', message)

  def testPathToDexForPlatformVersion_lowerApiLevelNoOdexFile(self):
    sdk_int = version_codes.MARSHMALLOW
    package_name = 'package.name'
    paths_to_apk = ['/some/path/{}-1/base.apk'.format(package_name)]
    arch = abis.ARM_64

    device = mock.Mock(build_version_sdk=sdk_int, product_cpu_abi=arch)
    device.GetApplicationPaths = mock.MagicMock(return_value=paths_to_apk)
    device.FileExists = mock.MagicMock(return_value=False)

    with self.assertRaises(list_verification.DeviceOSError) as _:
      list_verification.FindOdexFiles(device, package_name)

  def testListClasses_noProguardMap(self):
    oatdump_output = [
        _CreateOdexLine('a.b.JavaClass1', 6, 'StatusVerified'),
        _CreateOdexLine('a.b.JavaClass2', 7,
                        'StatusRetryVerificationAtRuntime'),
    ]

    classes = list_verification.ParseOatdump(oatdump_output, None)
    self.assertEqual(2, len(classes))
    java_class_1 = _ClassForName('a.b.JavaClass1', classes)
    java_class_2 = _ClassForName('a.b.JavaClass2', classes)
    self.assertEqual(java_class_1.verification_status, 'Verified')
    self.assertEqual(java_class_2.verification_status,
                     'RetryVerificationAtRuntime')

  def testListClasses_proguardMap(self):
    oatdump_output = [
        _CreateOdexLine('a.b.ObfuscatedJavaClass1', 6, 'StatusVerified'),
        _CreateOdexLine('a.b.ObfuscatedJavaClass2', 7,
                        'StatusRetryVerificationAtRuntime'),
    ]

    mapping = {
        'a.b.ObfuscatedJavaClass1': 'a.b.JavaClass1',
        'a.b.ObfuscatedJavaClass2': 'a.b.JavaClass2',
    }
    classes = list_verification.ParseOatdump(oatdump_output, mapping)
    self.assertEqual(2, len(classes))
    java_class_1 = _ClassForName('a.b.JavaClass1', classes)
    java_class_2 = _ClassForName('a.b.JavaClass2', classes)
    self.assertEqual(java_class_1.verification_status, 'Verified')
    self.assertEqual(java_class_2.verification_status,
                     'RetryVerificationAtRuntime')

  def testListClasses_noStatusPrefix(self):
    oatdump_output = [
        _CreateOdexLine('a.b.JavaClass1', 6, 'Verified'),
        _CreateOdexLine('a.b.JavaClass2', 7, 'RetryVerificationAtRuntime'),
    ]

    classes = list_verification.ParseOatdump(oatdump_output, None)
    self.assertEqual(2, len(classes))
    java_class_1 = _ClassForName('a.b.JavaClass1', classes)
    java_class_2 = _ClassForName('a.b.JavaClass2', classes)
    self.assertEqual(java_class_1.verification_status, 'Verified')
    self.assertEqual(java_class_2.verification_status,
                     'RetryVerificationAtRuntime')

if __name__ == '__main__':
  # Suppress logging messages.
  unittest.main(buffer=True)

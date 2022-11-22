#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests scenarios with number of devices and invalid devices"""
import common
import os
import subprocess
import time
import unittest
import unittest.mock as mock

from argparse import Namespace
from device_target import DeviceTarget
from legacy_ermine_ctl import LegacyErmineCtl
from ffx_session import FfxRunner, FfxTarget
from target import Target, FuchsiaTargetException


@mock.patch.object(FfxRunner, 'daemon_stop')
class TestDiscoverDeviceTarget(unittest.TestCase):
  def setUp(self):
    self.args = Namespace(out_dir='out/fuchsia',
                          target_cpu='x64',
                          host=None,
                          node_name=None,
                          port=None,
                          ssh_config='mock_config',
                          fuchsia_out_dir=None,
                          os_check='ignore',
                          logs_dir=None,
                          system_image_dir=None)

  def testUnspecifiedNodeNameOneDeviceReturnNoneCheckNameAndAddress(
      self, mock_daemon_stop):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(
             DeviceTarget, '_ConnectToTarget') as mock_connecttotarget, \
         mock.patch.object(
             DeviceTarget, '_Login') as mock_login:
      mock_list_targets.return_value = [{
          "nodename": "device_name",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
      }]
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_connecttotarget.return_value = True
      self.assertIsNone(device_target_instance.Start())
      self.assertEqual(device_target_instance._host, 'address')
      self.assertEqual(device_target_instance._port, 12345)
    mock_daemon_stop.assert_called_once()

  def testUnspecifiedNodeNameOneUnknownDeviceReturnNoneCheckAddressAndPort(
      self, mock_daemon_stop):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(
             DeviceTarget, '_ConnectToTarget') as mock_connecttotarget, \
         mock.patch.object(
             DeviceTarget, '_Login') as mock_login:
      mock_list_targets.return_value = [{
          "nodename": "<unknown>",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address"]
      }]
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_connecttotarget.return_value = True
      self.assertIsNone(device_target_instance.Start())
      self.assertEqual(device_target_instance._host, 'address')
      self.assertEqual(device_target_instance._port, 12345)
      mock_login.assert_called_once()
    mock_daemon_stop.assert_called_once()

  def testUnspecifiedNodeNameTwoDevicesRaiseExceptionAmbiguousTarget(
      self, mock_daemon_stop):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         mock.patch.object(
           FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         self.assertRaisesRegex(Exception, \
                                'More than one device was discovered'):
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_list_targets.return_value = [{
          "nodename": "device_name1",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address1"]
      }, {
          "nodename": "device_name2",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address2"]
      }]
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)
    mock_daemon_stop.assert_called_once()

  def testNodeNameDefinedDeviceFoundReturnNoneCheckNameAndHost(
      self, mock_daemon_stop):
    self.args.node_name = 'device_name'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(
             DeviceTarget, '_ConnectToTarget') as mock_connecttotarget, \
         mock.patch.object(
             DeviceTarget, '_Login') as mock_login:
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_connecttotarget.return_value = True
      self.assertIsNone(device_target_instance.Start())
      self.assertEqual(device_target_instance._node_name, 'device_name')
      self.assertEqual(device_target_instance._host, 'address')
      self.assertEqual(device_target_instance._port, 12345)
      mock_login.assert_called_once()
    mock_daemon_stop.assert_called_once()

  def testNodeNameDefinedDeviceNotFoundRaiseExceptionCouldNotFind(
      self, mock_daemon_stop):
    self.args.node_name = 'wrong_device_name'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         self.assertRaisesRegex(Exception, 'Could not find device.'):
      mock_get_ssh_address.return_value = None
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)
    mock_daemon_stop.assert_called_once()

  def testNoDevicesFoundRaiseExceptionCouldNotFind(self, mock_daemon_stop):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         self.assertRaisesRegex(Exception, 'Could not find device.'):
      mock_list_targets.return_value = []
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)
    mock_daemon_stop.assert_called_once()

  @mock.patch('os.path.exists', return_value=True)
  def testNoProvisionDeviceIfVersionsMatch(self, unused_mock, mock_daemon_stop):
    self.args.os_check = 'update'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(DeviceTarget, '_Discover') as mock_discover, \
         mock.patch.object(DeviceTarget, '_ConnectToTarget') as mock_connect, \
         mock.patch('device_target.get_sdk_hash') as mock_hash, \
         mock.patch.object(
            DeviceTarget, '_GetInstalledSdkVersion') as mock_version, \
         mock.patch.object(
             DeviceTarget, '_ProvisionDevice') as mock_provision, \
         mock.patch.object(
             DeviceTarget, '_Login') as mock_login:
      mock_discover.return_value = True
      mock_hash.return_value = '1.0'
      mock_version.return_value = '1.0'
      device_target_instance.Start()
      self.assertEqual(mock_provision.call_count, 0)
      mock_login.assert_called_once()
    mock_daemon_stop.assert_called_once()

  @mock.patch('os.path.exists', return_value=True)
  def testRaiseExceptionIfCheckVersionsNoMatch(self, unused_mock,
                                               mock_daemon_stop):
    self.args.os_check = 'check'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(DeviceTarget, '_Discover') as mock_discover, \
         mock.patch.object(DeviceTarget, '_ConnectToTarget') as mock_ready, \
         mock.patch('device_target.get_sdk_hash') as mock_hash, \
         mock.patch.object(
            DeviceTarget, '_GetInstalledSdkVersion') as mock_version, \
         mock.patch.object(
            DeviceTarget, '_ProvisionDevice') as mock_provision, \
         self.assertRaisesRegex(Exception, 'Image and Fuchsia version'):
      mock_discover.return_value = True
      mock_hash.return_value = '2.0'
      mock_version.return_value = '1.0'
      device_target_instance.Start()
    mock_daemon_stop.assert_called_once()

  def testLoginCallsOnlyIfErmineExists(self, mock_daemon_stop):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             LegacyErmineCtl, 'exists',
             new_callable=mock.PropertyMock) as mock_exists, \
         mock.patch.object(LegacyErmineCtl, 'take_to_shell') as mock_shell:
      mock_exists.return_value = True

      device_target_instance._Login()

      mock_exists.assert_called_once()
      mock_shell.assert_called_once()

    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             LegacyErmineCtl, 'exists',
             new_callable=mock.PropertyMock) as mock_exists, \
         mock.patch.object(LegacyErmineCtl, 'take_to_shell') as mock_shell:
      mock_exists.return_value = False

      device_target_instance._Login()

      mock_exists.assert_called_once()
      self.assertEqual(mock_shell.call_count, 0)

  @mock.patch('os.path.exists', return_value=True)
  def testProvisionIfOneNonDetectableDevice(self, unused_mock,
                                            mock_daemon_stop):
    self.args.os_check = 'update'
    self.args.node_name = 'mocknode'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(DeviceTarget,
                           '_ProvisionDevice') as mock_provision, \
         mock.patch.object(DeviceTarget, '_Login') as mock_bypass:
      mock_get_ssh_address.return_value = None
      device_target_instance.Start()
      self.assertEqual(mock_provision.call_count, 1)
    mock_daemon_stop.assert_called_once()

  def testRaiseExceptionIfNoTargetDir(self, mock_daemon_stop):
    self.args.os_check = 'update'
    self.args.system_image_dir = ''
    with self.assertRaises(Exception):
      DeviceTarget.CreateFromArgs(self.args)

  def testSearchSDKIfImageDirNotFound(self, mock_daemon_stop):
    self.args.os_check = 'update'
    self.args.system_image_dir = 'product-bundle-instead-of-image'
    with mock.patch('os.path.exists', return_value=False), \
        mock.patch('device_target.find_image_in_sdk',
                   return_value='some/path/to/image') as mock_find, \
        mock.patch('device_target.SDK_ROOT', 'some/path/to/sdk'), \
        self.assertLogs():
      target = DeviceTarget.CreateFromArgs(self.args)
      mock_find.assert_called_once_with('product-bundle-instead-of-image',
                                        product_bundle=True,
                                        sdk_root='some/path/to')
      self.assertEqual(target._system_image_dir, 'some/path/to/image')

  def testSearchSDKThrowsExceptionIfNoPathReturned(self, mock_daemon_stop):
    self.args.os_check = 'update'
    self.args.system_image_dir = 'product-bundle-instead-of-image'
    with mock.patch('os.path.exists', return_value=False), \
        mock.patch('device_target.find_image_in_sdk',
                   return_value=None), \
        mock.patch('device_target.SDK_ROOT', 'some/path/to/sdk'), \
        self.assertLogs(), \
        self.assertRaises(FileNotFoundError):
      target = DeviceTarget.CreateFromArgs(self.args)


if __name__ == '__main__':
  unittest.main()

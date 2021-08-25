#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests scenarios with number of devices and invalid devices"""
import subprocess
import unittest
import unittest.mock as mock
from argparse import Namespace
from device_target import DeviceTarget
from target import Target


class TestDiscoverDeviceTarget(unittest.TestCase):
  def setUp(self):
    self.args = Namespace(out_dir='out/fuchsia',
                          target_cpu='x64',
                          host=None,
                          node_name=None,
                          port=None,
                          ssh_config=None,
                          fuchsia_out_dir=None,
                          os_check='update',
                          system_log_file=None)

  def testNoNodeNameOneDeviceReturnNoneCheckNameAndAddress(self):
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch.object(DeviceTarget, 'RunFFXCommand') as mock_ffx:
        mock_spec_popen = mock.create_autospec(subprocess.Popen, instance=True)
        mock_spec_popen.communicate.return_value = ('address device_name', '')
        mock_spec_popen.returncode = 0
        mock_ffx.return_value = mock_spec_popen
        with mock.patch.object(Target,
                               '_WaitUntilReady') as mock_waituntilready:
          mock_waituntilready.return_value = True
          self.assertIsNone(device_target_instance.Start())
          self.assertEqual(device_target_instance._node_name, 'device_name')
          self.assertEqual(device_target_instance._host, 'address')

  def testNoNodeNameTwoDevicesRaiseExceptionAmbiguousTarget(self):
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch.object(DeviceTarget, 'RunFFXCommand') as mock_ffx:
        mock_spec_popen = mock.create_autospec(subprocess.Popen, instance=True)
        mock_spec_popen.communicate.return_value = ('address1 device_name1\n'
                                                    'address2 device_name2', '')
        mock_spec_popen.returncode = 0
        mock_spec_popen.stdout = ''
        mock_ffx.return_value = mock_spec_popen
        with self.assertRaisesRegex(Exception,
                                    'Ambiguous target device specification.'):
          device_target_instance.Start()
          self.assertIsNone(device_target_instance._node_name)
          self.assertIsNone(device_target_instance._host)

  def testNoNodeNameDeviceDoesntHaveNameRaiseExceptionCouldNotFind(self):
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch.object(DeviceTarget, 'RunFFXCommand') as mock_ffx:
        mock_spec_popen = mock.create_autospec(subprocess.Popen, instance=True)
        mock_spec_popen.communicate.return_value = ('address', '')
        mock_spec_popen.returncode = 0
        mock_ffx.return_value = mock_spec_popen
        with self.assertRaisesRegex(Exception, 'Could not find device'):
          device_target_instance.Start()
          self.assertIsNone(device_target_instance._node_name)
          self.assertIsNone(device_target_instance._host)

  def testNodeNameDefinedDeviceFoundReturnNoneCheckNameAndHost(self):
    self.args.node_name = 'device_name'
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch('subprocess.Popen') as mock_popen:
        mock_popen.returncode = ('address', 'device_name')
        with mock.patch.object(Target,
                               '_WaitUntilReady') as mock_waituntilready:
          mock_waituntilready.return_value = True
          self.assertIsNone(device_target_instance.Start())
          self.assertEqual(device_target_instance._node_name, 'device_name')
          self.assertEqual(device_target_instance._host, 'address')

  def testNodeNameDefinedDeviceNotFoundRaiseExceptionCouldNotFind(self):
    self.args.node_name = 'wrong_device_name'
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch('subprocess.Popen') as mock_popen:
        mock_popen.returncode = ('', '')
        with self.assertRaisesRegex(Exception, 'Could not find device'):
          device_target_instance.Start()
          self.assertIsNone(device_target_instance._node_name)
          self.assertIsNone(device_target_instance._host)

  def testNoDevicesFoundRaiseExceptionCouldNotFind(self):
    with (DeviceTarget.CreateFromArgs(self.args)) as device_target_instance:
      with mock.patch.object(DeviceTarget, 'RunFFXCommand') as mock_ffx:
        mock_spec_popen = mock.create_autospec(subprocess.Popen, instance=True)
        mock_spec_popen.communicate.return_value = ('', '')
        mock_spec_popen.returncode = 0
        mock_ffx.return_value = mock_spec_popen
        with self.assertRaisesRegex(Exception, 'Could not find device'):
          device_target_instance.Start()
          self.assertIsNone(device_target_instance._node_name)
          self.assertIsNone(device_target_instance._host)


if __name__ == '__main__':
  unittest.main()

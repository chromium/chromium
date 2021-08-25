#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests different flags to see if they are being used correctly"""

import boot_data
import common
import os
import unittest
import unittest.mock as mock

from argparse import Namespace
from fvdl_target import FvdlTarget, _SSH_KEY_DIR


class TestBuildCommandFvdlTarget(unittest.TestCase):
  def setUp(self):
    self.args = Namespace(out_dir='outdir',
                          system_log_file=None,
                          target_cpu='x64',
                          require_kvm=True,
                          enable_graphics=False,
                          hardware_gpu=False,
                          with_network=False,
                          ram_size_mb=8192)

  def testBasicEmuCommand(self):
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        build_command = target._BuildCommand()
        self.assertIn(target._FVDL_PATH, build_command)
        self.assertIn('--sdk', build_command)
        self.assertIn('start', build_command)
        self.assertNotIn('--noacceleration', target._BuildCommand())
        self.assertIn('--headless', target._BuildCommand())
        self.assertNotIn('--host-gpu', target._BuildCommand())
        self.assertNotIn('-N', target._BuildCommand())
        self.assertIn('--device-proto', target._BuildCommand())
        self.assertTrue(os.path.exists(target._device_proto_file.name))
        correct_ram_amount = False
        with open(target._device_proto_file.name) as file:
          for line in file:
            if line.strip() == 'ram:  8192':
              correct_ram_amount = True
              break
        self.assertTrue(correct_ram_amount)

  def testBuildCommandCheckIfNotRequireKVMSetNoAcceleration(self):
    self.args.require_kvm = False
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        self.assertIn('--noacceleration', target._BuildCommand())

  def testBuildCommandCheckIfNotEnableGraphicsSetHeadless(self):
    self.args.enable_graphics = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        self.assertNotIn('--headless', target._BuildCommand())

  def testBuildCommandCheckIfHardwareGpuSetHostGPU(self):
    self.args.hardware_gpu = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        self.assertIn('--host-gpu', target._BuildCommand())

  def testBuildCommandCheckIfWithNetworkSetTunTap(self):
    self.args.with_network = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        self.assertIn('-N', target._BuildCommand())

  def testBuildCommandCheckRamSizeNot8192SetRamSize(self):
    self.args.ram_size_mb = 4096
    with FvdlTarget.CreateFromArgs(self.args) as target:
      target.Shutdown = mock.MagicMock()
      common.EnsurePathExists = mock.MagicMock(return_value='image')
      with mock.patch.object(boot_data, 'ProvisionSSH') as provision_mock:
        self.assertIn('--device-proto', target._BuildCommand())
        self.assertTrue(os.path.exists(target._device_proto_file.name))
        correct_ram_amount = False
        with open(target._device_proto_file.name) as file:
          for line in file:
            if line.strip() == 'ram:  4096':
              correct_ram_amount = True
              break
        self.assertTrue(correct_ram_amount)


if __name__ == '__main__':
  unittest.main()

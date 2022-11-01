#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests different flags to see if they are being used correctly"""

import boot_data
import common
import os
import tempfile
import unittest
import unittest.mock as mock

from argparse import Namespace
from ffx_session import FfxRunner
from fvdl_target import FvdlTarget, _SSH_KEY_DIR

_EMU_METADATA = {
    "disk_images": ["fuchsia.blk"],
    "initial_ramdisk": "fuchsia.zbi",
    "kernel": "fuchsia.bin"
}


@mock.patch.object(FfxRunner, 'daemon_stop')
class TestBuildCommandFvdlTarget(unittest.TestCase):
  def setUp(self):
    self.args = Namespace(out_dir='outdir',
                          system_log_file=None,
                          target_cpu='x64',
                          require_kvm=True,
                          enable_graphics=False,
                          hardware_gpu=False,
                          with_network=False,
                          ram_size_mb=8192,
                          logs_dir=None,
                          custom_image=None,
                          cpu_cores=10)
    common.EnsurePathExists = mock.MagicMock(return_value='image')
    boot_data.ProvisionSSH = mock.MagicMock()
    FvdlTarget._GetPbPath = mock.MagicMock(return_value='path')
    FvdlTarget._GetEmuMetadata = mock.MagicMock(return_value=_EMU_METADATA)
    FvdlTarget._Shutdown = mock.MagicMock()

  def testBasicEmuCommand(self, mock_daemon_stop):
    with FvdlTarget.CreateFromArgs(self.args) as target:
      build_command = target._BuildCommand()
      self.assertIn(target._FVDL_PATH, build_command)
      self.assertIn('--sdk', build_command)
      self.assertIn('start', build_command)
      self.assertNotIn('--noacceleration', build_command)
      self.assertIn('--headless', build_command)
      self.assertNotIn('--host-gpu', build_command)
      self.assertNotIn('-N', build_command)
      self.assertIn('--device-proto', build_command)
      self.assertNotIn('--emulator-log', build_command)
      self.assertNotIn('--envs', build_command)
      self.assertTrue(os.path.exists(target._device_proto_file.name))
      correct_ram_amount = False
      with open(target._device_proto_file.name) as file:
        for line in file:
          if line.strip() == 'ram:  8192':
            correct_ram_amount = True
            break
      self.assertTrue(correct_ram_amount)
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckIfNotRequireKVMSetNoAcceleration(
      self, mock_daemon_stop):
    self.args.require_kvm = False
    with FvdlTarget.CreateFromArgs(self.args) as target:
      self.assertIn('--noacceleration', target._BuildCommand())
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckIfNotEnableGraphicsSetHeadless(
      self, mock_daemon_stop):
    self.args.enable_graphics = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      self.assertNotIn('--headless', target._BuildCommand())
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckIfHardwareGpuSetHostGPU(self, mock_daemon_stop):
    self.args.hardware_gpu = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      self.assertIn('--host-gpu', target._BuildCommand())
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckIfWithNetworkSetTunTap(self, mock_daemon_stop):
    self.args.with_network = True
    with FvdlTarget.CreateFromArgs(self.args) as target:
      self.assertIn('-N', target._BuildCommand())
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckRamSizeNot8192SetRamSize(self, mock_daemon_stop):
    custom_ram_size = 4096
    self.args.ram_size_mb = custom_ram_size
    with FvdlTarget.CreateFromArgs(self.args) as target:
      self.assertIn('--device-proto', target._BuildCommand())
      self.assertTrue(os.path.exists(target._device_proto_file.name))
      correct_ram_amount = False
      with open(target._device_proto_file.name, 'r') as f:
        self.assertTrue('  ram:  {}\n'.format(custom_ram_size) in f.readlines())
    mock_daemon_stop.assert_called_once()

  def testBuildCommandCheckEmulatorLogSetup(self, mock_daemon_stop):
    with tempfile.TemporaryDirectory() as logs_dir:
      self.args.logs_dir = logs_dir
      with FvdlTarget.CreateFromArgs(self.args) as target:
        build_command = target._BuildCommand()
        self.assertIn('--emulator-log', build_command)
        self.assertIn('--envs', build_command)
      mock_daemon_stop.assert_called_once()


if __name__ == '__main__':
  unittest.main()

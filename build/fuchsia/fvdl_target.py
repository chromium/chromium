# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running and interacting with Fuchsia on FVDL."""

import boot_data
import common
import emu_target
import logging
import os
import subprocess
import tempfile

_FVDL_PATH = os.path.join(common.SDK_ROOT, 'tools', 'x64', 'fvdl')
_SSH_KEY_DIR = os.path.expanduser('~/.ssh')


def GetTargetType():
  return FvdlTarget


class FvdlTarget(emu_target.EmuTarget):
  EMULATOR_NAME = 'fvdl'

  def __init__(self,
               out_dir,
               target_cpu,
               system_log_file,
               require_kvm,
               enable_graphics,
               hardware_gpu,
               fuchsia_out_dir=None):
    super(FvdlTarget, self).__init__(out_dir, target_cpu, system_log_file,
                                     fuchsia_out_dir)
    self._require_kvm = require_kvm
    self._enable_graphics = enable_graphics
    self._hardware_gpu = hardware_gpu
    self._emulator_pid = None

    # Use a temp file for vdl output.
    self._vdl_output_file = tempfile.NamedTemporaryFile()

  @staticmethod
  def CreateFromArgs(args):
    return FvdlTarget(args.out_dir, args.target_cpu, args.system_log_file,
                      args.require_kvm, args.enable_graphics,
                      args.hardware_gpu, args.fuchsia_out_dir)

  def _BuildCommand(self):
    boot_data.ProvisionSSH(_SSH_KEY_DIR)
    self._host_ssh_port = common.GetAvailableTcpPort()
    kernel_image = common.EnsurePathExists(
        boot_data.GetTargetFile('qemu-kernel.kernel', self._GetTargetSdkArch(),
                                boot_data.TARGET_TYPE_QEMU))
    zbi_image = common.EnsurePathExists(
        boot_data.GetTargetFile('zircon-a.zbi', self._GetTargetSdkArch(),
                                boot_data.TARGET_TYPE_QEMU))
    fvm_image = common.EnsurePathExists(
        boot_data.GetTargetFile('storage-full.blk', self._GetTargetSdkArch(),
                                boot_data.TARGET_TYPE_QEMU))
    emu_command = [
        _FVDL_PATH, '--sdk', 'start', '--nopackageserver', '--nointeractive',
        '--port-map', 'hostfwd=tcp::{}-:22'.format(self._host_ssh_port),
        '--vdl-output', self._vdl_output_file.name, '--kernel-image',
        kernel_image, '--zbi-image', zbi_image, '--fvm-image', fvm_image,
        '--image-architecture', 'x64'
    ]

    if not self._require_kvm:
      emu_command.append('--noacceleration')
    if not self._enable_graphics:
      emu_command.append('--headless')
    if self._hardware_gpu:
      emu_command.append('--host-gpu')
    logging.info(emu_command)

    return emu_command

  def _GetEndpoint(self):
    return ('localhost', self._host_ssh_port)

  def Shutdown(self):
    if not self._emu_process:
      logging.error('%s did not start' % (self.EMULATOR_NAME))
      return
    femu_command = [
        _FVDL_PATH, '--sdk', 'kill', '--launched-proto',
        self._vdl_output_file.name
    ]
    femu_process = subprocess.Popen(femu_command)
    returncode = femu_process.wait()
    if returncode == 0:
      logging.info('FVDL shutdown successfully')
    else:
      logging.info('FVDL kill returned error status {}'.format(returncode))
    emu_target.LogProcessStatistics('proc_stat_end_log')
    emu_target.LogSystemStatistics('system_statistics_end_log')
    self._vdl_output_file.close()

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath(_SSH_KEY_DIR)

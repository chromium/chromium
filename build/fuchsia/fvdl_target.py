# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running and interacting with Fuchsia on FVDL."""

import boot_data
import common
import emu_target
import logging
import os
import re
import subprocess
import tempfile

_FVDL_PATH = os.path.join(common.SDK_ROOT, 'tools', 'x64', 'fvdl')
_SSH_KEY_DIR = os.path.expanduser('~/.ssh')
_DEFAULT_SSH_PORT = 22


def GetTargetType():
  return FvdlTarget


class EmulatorNetworkNotFoundError(Exception):
  """Raised when emulator's address cannot be found"""
  pass


class FvdlTarget(emu_target.EmuTarget):
  EMULATOR_NAME = 'aemu'

  def __init__(self, out_dir, target_cpu, system_log_file, require_kvm,
               enable_graphics, hardware_gpu, with_network):
    super(FvdlTarget, self).__init__(out_dir, target_cpu, system_log_file)
    self._require_kvm = require_kvm
    self._enable_graphics = enable_graphics
    self._hardware_gpu = hardware_gpu
    self._with_network = with_network

    self._host = None
    self._pid = None

    # Use a temp file for vdl output.
    self._vdl_output_file = tempfile.NamedTemporaryFile()

  @staticmethod
  def CreateFromArgs(args):
    return FvdlTarget(args.out_dir, args.target_cpu, args.system_log_file,
                      args.require_kvm, args.enable_graphics, args.hardware_gpu,
                      args.with_network)

  @staticmethod
  def RegisterArgs(arg_parser):
    fvdl_args = arg_parser.add_argument_group('fvdl', 'FVDL arguments')
    fvdl_args.add_argument('--with-network',
                           action='store_true',
                           default=False,
                           help='Run emulator with emulated nic via tun/tap.')

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
    aemu_path = common.EnsurePathExists(
        os.path.join(common.GetEmuRootForPlatform(self.EMULATOR_NAME),
                     'emulator'))

    emu_command = [
        _FVDL_PATH,
        '--sdk',
        'start',
        '--nopackageserver',
        '--nointeractive',

        # Host port mapping for user-networking mode.
        '--port-map',
        'hostfwd=tcp::{}-:22'.format(self._host_ssh_port),

        # no-interactive requires a --vdl-output flag to shutdown the emulator.
        '--vdl-output',
        self._vdl_output_file.name,

        # Use existing images instead of downloading new ones.
        '--kernel-image',
        kernel_image,
        '--zbi-image',
        zbi_image,
        '--fvm-image',
        fvm_image,
        '--image-architecture',
        self._target_cpu,

        # Use an existing emulator checked out by Chromium.
        '--aemu-path',
        aemu_path
    ]

    if not self._require_kvm:
      emu_command.append('--noacceleration')
    if not self._enable_graphics:
      emu_command.append('--headless')
    if self._hardware_gpu:
      emu_command.append('--host-gpu')
    if self._with_network:
      emu_command.append('-N')
    logging.info('FVDL command: ' + ' '.join(emu_command))

    return emu_command

  def _WaitUntilReady(self):
    # Indicates the FVDL command finished running.
    self._emu_process.communicate()
    super(FvdlTarget, self)._WaitUntilReady()

  def _IsEmuStillRunning(self):
    if not self._pid:
      try:
        with open(self._vdl_output_file.name) as vdl_file:
          for line in vdl_file:
            if 'pid' in line:
              match = re.match(r'.*pid:\s*(\d*).*', line)
              if match:
                self._pid = match.group(1)
      except IOError:
        logging.error('vdl_output file no longer found. '
                      'Cannot get emulator pid.')
        return False
    if subprocess.check_output(['ps', '-p', self._pid, 'o', 'comm=']):
      return True
    logging.error('Emulator pid no longer found. Emulator must be down.')
    return False

  def _GetEndpoint(self):
    if self._with_network:
      return self._GetNetworkAddress()
    return ('localhost', self._host_ssh_port)

  def _GetNetworkAddress(self):
    if self._host:
      return (self._host, _DEFAULT_SSH_PORT)
    try:
      with open(self._vdl_output_file.name) as vdl_file:
        for line in vdl_file:
          if 'network_address' in line:
            address = re.match(r'.*network_address:\s*"\[(.*)\]".*', line)
            if address:
              self._host = address.group(1)
              return (self._host, _DEFAULT_SSH_PORT)
        logging.error('Network address not found.')
        raise EmulatorNetworkNotFoundError()
    except IOError as e:
      logging.error('vdl_output file not found. Cannot get network address.')
      raise

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

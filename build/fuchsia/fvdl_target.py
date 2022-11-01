# Copyright 2021 The Chromium Authors
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

_SSH_KEY_DIR = os.path.expanduser('~/.ssh')
_DEFAULT_SSH_PORT = 22
_DEVICE_PROTO_TEMPLATE = """
device_spec:  {{
  horizontal_resolution:  1024
  vertical_resolution:  600
  vm_heap:  192
  ram:  {ramsize}
  cache:  32
  screen_density:  240
}}
"""


def GetTargetType():
  return FvdlTarget


class EmulatorNetworkNotFoundError(Exception):
  """Raised when emulator's address cannot be found"""
  pass


class FvdlTarget(emu_target.EmuTarget):
  EMULATOR_NAME = 'aemu'
  _FVDL_PATH = os.path.join(common.SDK_ROOT, 'tools', 'x64', 'fvdl')

  def __init__(self, out_dir, target_cpu, require_kvm, enable_graphics,
               hardware_gpu, with_network, cpu_cores, ram_size_mb, logs_dir,
               custom_image):

    super(FvdlTarget, self).__init__(out_dir, target_cpu, logs_dir,
                                     custom_image)
    self._require_kvm = require_kvm
    self._enable_graphics = enable_graphics
    self._hardware_gpu = hardware_gpu
    self._with_network = with_network
    self._cpu_cores = cpu_cores
    self._ram_size_mb = ram_size_mb

    self._host = None
    self._pid = None

    # Use a temp file for vdl output.
    self._vdl_output_file = tempfile.NamedTemporaryFile()

    # Use a temp file for the device proto and write the ram size.
    self._device_proto_file = tempfile.NamedTemporaryFile()
    with open(self._device_proto_file.name, 'w') as file:
      file.write(_DEVICE_PROTO_TEMPLATE.format(ramsize=self._ram_size_mb))

  @staticmethod
  def CreateFromArgs(args):
    return FvdlTarget(args.out_dir, args.target_cpu, args.require_kvm,
                      args.enable_graphics, args.hardware_gpu,
                      args.with_network, args.cpu_cores, args.ram_size_mb,
                      args.logs_dir, args.custom_image)

  @staticmethod
  def RegisterArgs(arg_parser):
    fvdl_args = arg_parser.add_argument_group('fvdl', 'FVDL arguments')
    fvdl_args.add_argument('--with-network',
                           action='store_true',
                           default=False,
                           help='Run emulator with emulated nic via tun/tap.')
    fvdl_args.add_argument('--custom-image',
                           help='Specify an image used for booting up the '\
                                'emulator.')
    fvdl_args.add_argument('--enable-graphics',
                           action='store_true',
                           default=False,
                           help='Start emulator with graphics instead of '\
                                'headless.')
    fvdl_args.add_argument('--hardware-gpu',
                           action='store_true',
                           default=False,
                           help='Use local GPU hardware instead Swiftshader.')

  def _BuildCommand(self):
    boot_data.ProvisionSSH()
    self._host_ssh_port = common.GetAvailableTcpPort()
    kernel_image = common.EnsurePathExists(
        boot_data.GetTargetFile(self._kernel, self._pb_path))
    zbi_image = common.EnsurePathExists(
        boot_data.GetTargetFile(self._ramdisk, self._pb_path))
    fvm_image = common.EnsurePathExists(
        boot_data.GetTargetFile(self._disk_image, self._pb_path))
    aemu_path = common.EnsurePathExists(
        os.path.join(common.GetHostToolPathFromPlatform('aemu_internal'),
                     'emulator'))
    emu_command = [
        self._FVDL_PATH,
        '--sdk',
        'start',
        '--nointeractive',

        # Host port mapping for user-networking mode.
        '--port-map',
        'hostfwd=tcp::{}-:22'.format(self._host_ssh_port),

        # no-interactive requires a --vdl-output flag to shutdown the emulator.
        '--vdl-output',
        self._vdl_output_file.name,
        '-c',
        ' '.join(boot_data.GetKernelArgs()),

        # Use an existing emulator checked out by Chromium.
        '--aemu-path',
        aemu_path,

        # Use existing images instead of downloading new ones.
        '--kernel-image',
        kernel_image,
        '--zbi-image',
        zbi_image,
        '--fvm-image',
        fvm_image,
        '--image-architecture',
        self._target_cpu,

        # Use this flag and temp file to define ram size.
        '--device-proto',
        self._device_proto_file.name,
        '--cpu-count',
        str(self._cpu_cores)
    ]
    self._ConfigureEmulatorLog(emu_command)

    if not self._require_kvm:
      emu_command.append('--noacceleration')
    if not self._enable_graphics:
      emu_command.append('--headless')
    if self._hardware_gpu:
      emu_command.append('--host-gpu')
    if self._with_network:
      emu_command.append('-N')

    return emu_command

  def _ConfigureEmulatorLog(self, emu_command):
    if self._log_manager.IsLoggingEnabled():
      emu_command.extend([
          '--emulator-log',
          os.path.join(self._log_manager.GetLogDirectory(), 'emulator_log')
      ])

      env_flags = [
          'ANDROID_EMUGL_LOG_PRINT=1',
          'ANDROID_EMUGL_VERBOSE=1',
          'VK_LOADER_DEBUG=info,error',
      ]
      if self._hardware_gpu:
        vulkan_icd_file = os.path.join(
            common.GetHostToolPathFromPlatform('aemu_internal'), 'lib64',
            'vulkan', 'vk_swiftshader_icd.json')
        env_flags.append('VK_ICD_FILENAMES=%s' % vulkan_icd_file)
      for flag in env_flags:
        emu_command.extend(['--envs', flag])

  def _HasNetworking(self):
    return self._with_network

  def _ConnectToTarget(self):
    # Wait for the emulator to finish starting up.
    logging.info('Waiting for fvdl to start...')
    self._emu_process.communicate()
    super(FvdlTarget, self)._ConnectToTarget()

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
    try:
      if subprocess.check_output(['ps', '-p', self._pid, 'o', 'comm=']):
        return True
    except subprocess.CalledProcessError:
      # The process must be gone.
      pass
    logging.error('Emulator pid no longer found. Emulator must be down.')
    return False

  def _GetEndpoint(self):
    if self._with_network:
      return self._GetNetworkAddress()
    return (self.LOCAL_ADDRESS, self._host_ssh_port)

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

  def _Shutdown(self):
    if not self._emu_process:
      logging.error('%s did not start' % (self.EMULATOR_NAME))
      return
    femu_command = [
        self._FVDL_PATH, '--sdk', 'kill', '--launched-proto',
        self._vdl_output_file.name
    ]
    femu_process = subprocess.Popen(femu_command)
    returncode = femu_process.wait()
    if returncode == 0:
      logging.info('FVDL shutdown successfully')
    else:
      logging.info('FVDL kill returned error status {}'.format(returncode))
    self.LogProcessStatistics('proc_stat_end_log')
    self.LogSystemStatistics('system_statistics_end_log')
    self._vdl_output_file.close()
    self._device_proto_file.close()

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath()

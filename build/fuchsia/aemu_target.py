# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on AEMU."""

import emu_target
import os
import platform
import qemu_target
import logging

from common import GetEmuRootForPlatform


def GetTargetType():
  return AemuTarget


class AemuTarget(qemu_target.QemuTarget):
  EMULATOR_NAME = 'aemu'

  def __init__(self, out_dir, target_cpu, system_log_file, cpu_cores,
               require_kvm, ram_size_mb, enable_graphics, hardware_gpu):
    super(AemuTarget, self).__init__(out_dir, target_cpu, system_log_file,
                                     cpu_cores, require_kvm, ram_size_mb)

    self._enable_graphics = enable_graphics
    self._hardware_gpu = hardware_gpu

  @staticmethod
  def CreateFromArgs(args):
    return AemuTarget(args.out_dir, args.target_cpu, args.system_log_file,
                      args.cpu_cores, args.require_kvm, args.ram_size_mb,
                      args.enable_graphics, args.hardware_gpu)

  @staticmethod
  def RegisterArgs(arg_parser):
    aemu_args = arg_parser.add_argument_group('aemu', 'AEMU arguments')
    aemu_args.add_argument('--enable-graphics',
                           action='store_true',
                           default=False,
                           help='Start AEMU with graphics instead of '\
                                'headless.')
    aemu_args.add_argument('--hardware-gpu',
                           action='store_true',
                           default=False,
                           help='Use local GPU hardware instead of '\
                                'Swiftshader.')

  def _EnsureEmulatorExists(self, path):
    assert os.path.exists(path), \
          'This checkout is missing %s.' % (self.EMULATOR_NAME)

  def _BuildCommand(self):
    aemu_folder = GetEmuRootForPlatform(self.EMULATOR_NAME)

    self._EnsureEmulatorExists(aemu_folder)
    aemu_path = os.path.join(aemu_folder, 'emulator')

    # `VirtioInput` is needed for touch input device support on Fuchsia.
    # `RefCountPipe` is needed for proper cleanup of resources when a process
    # that uses Vulkan dies inside the guest
    aemu_features = 'VirtioInput,RefCountPipe'

    # Configure the CPU to emulate.
    # On Linux, we can enable lightweight virtualization (KVM) if the host and
    # guest architectures are the same.
    if self._IsKvmEnabled():
      aemu_features += ',KVM,GLDirectMem,Vulkan'
    else:
      if self._target_cpu != 'arm64':
        aemu_features += ',-GLDirectMem'

    # Use Swiftshader for Vulkan if requested
    gpu_target = 'swiftshader_indirect'
    if self._hardware_gpu:
      gpu_target = 'host'

    aemu_command = [aemu_path]
    if not self._enable_graphics:
      aemu_command.append('-no-window')
    # All args after -fuchsia flag gets passed to QEMU
    aemu_command.extend([
        '-feature', aemu_features, '-window-size', '1024x600', '-gpu',
        gpu_target, '-verbose', '-fuchsia'
    ])

    aemu_command.extend(self._BuildQemuConfig())

    aemu_command.extend([
      '-vga', 'none',
      '-device', 'virtio-keyboard-pci',
      '-device', 'virtio_input_multi_touch_pci_1',
      '-device', 'ich9-ahci,id=ahci'])
    if platform.machine() == 'x86_64':
      aemu_command.extend(['-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04'])

    logging.info(' '.join(aemu_command))
    return aemu_command

  def _GetVulkanIcdFile(self):
    return os.path.join(GetEmuRootForPlatform(self.EMULATOR_NAME), 'lib64',
                        'vulkan', 'vk_swiftshader_icd.json')

  def _SetEnv(self):
    env = os.environ.copy()
    aemu_logging_env = {
        "ANDROID_EMU_VK_NO_CLEANUP": "1",
        "ANDROID_EMUGL_LOG_PRINT": "1",
        "ANDROID_EMUGL_VERBOSE": "1",
        "VK_ICD_FILENAMES": self._GetVulkanIcdFile(),
        "VK_LOADER_DEBUG": "info,error",
    }
    env.update(aemu_logging_env)
    return env

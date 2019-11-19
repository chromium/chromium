# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on AEMU."""

import os
import platform
import qemu_target
import logging

from common import GetEmuRootForPlatform

class AemuTarget(qemu_target.QemuTarget):
  def __init__(self, output_dir, target_cpu, system_log_file, emu_type,
               cpu_cores, require_kvm, ram_size_mb):
    super(AemuTarget, self).__init__(output_dir, target_cpu, system_log_file,
                                     emu_type, cpu_cores, require_kvm,
                                     ram_size_mb)

    # TODO(crbug.com/1000907): Enable AEMU for arm64.
    if platform.machine() == 'aarch64':
      raise Exception('AEMU does not support arm64 hosts.')

  # TODO(bugs.fuchsia.dev/p/fuchsia/issues/detail?id=37301): Remove
  # once aemu is part of default fuchsia build
  def _EnsureEmulatorExists(self, path):
    assert os.path.exists(path), \
          'This checkout is missing %s. To check out the files, add this\n' \
          'entry to the "custon_vars" section of your .gclient file:\n\n' \
          '   "checkout_aemu": True\n\n' % (self._emu_type)

  def _BuildCommand(self):
    aemu_exec = 'emulator-headless'

    aemu_folder = GetEmuRootForPlatform(self._emu_type)

    self._EnsureEmulatorExists(aemu_folder)
    aemu_path = os.path.join(aemu_folder, aemu_exec)

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

    # All args after -fuchsia flag gets passed to QEMU
    aemu_command = [aemu_path,
        '-feature', aemu_features,
        '-window-size', '1024x600',
        '-gpu', 'swiftshader_indirect',
        '-fuchsia'
    ]

    aemu_command.extend(self._BuildQemuConfig())

    aemu_command.extend([
      '-vga', 'none',
      '-device', 'isa-debug-exit,iobase=0xf4,iosize=0x04',
      '-device', 'virtio-keyboard-pci',
      '-device', 'virtio_input_multi_touch_pci_1',
      '-device', 'ich9-ahci,id=ahci'])
    logging.info(' '.join(aemu_command))
    return aemu_command

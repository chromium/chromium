# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implements commands for running and interacting with Fuchsia generic
build on devices."""

import boot_data
import device_target
import logging
import os

from common import SDK_ROOT, EnsurePathExists, \
                   GetHostToolPathFromPlatform, SubprocessCallWithTimeout


def GetTargetType():
  return GenericX64PavedDeviceTarget


class GenericX64PavedDeviceTarget(device_target.DeviceTarget):
  """In addition to the functionality provided by DeviceTarget, this class
  automatically handles paving of x64 devices that use generic Fuchsia build.

  If there are no running devices, then search for a device running Zedboot
  and pave it.

  If there's only one running device, or |_node_name| is set, then the
  device's SDK version is checked unless --os-check=ignore is set.
  If --os-check=update is set, then the target device is repaved if the SDK
  version doesn't match."""

  def _ProvisionDeviceIfNecessary(self):
    should_provision = False

    if self._Discover():
      self._WaitUntilReady()

      if self._os_check != 'ignore':
        if self._SDKHashMatches():
          if self._os_check == 'update':
            logging.info('SDK hash does not match; rebooting and repaving.')
            self.RunCommand(['dm', 'reboot'])
            should_provision = True
          elif self._os_check == 'check':
            raise Exception('Target device SDK version does not match.')
    else:
      should_provision = True

    if should_provision:
      self._ProvisionDevice()

  def _ProvisionDevice(self):
    """Pave a device with a generic image of Fuchsia."""
    bootserver_path = GetHostToolPathFromPlatform('bootserver')
    bootserver_command = [
        bootserver_path, '-1', '--fvm',
        EnsurePathExists(
            boot_data.GetTargetFile('storage-sparse.blk',
                                    self._GetTargetSdkArch(),
                                    boot_data.TARGET_TYPE_GENERIC)),
        EnsurePathExists(
            boot_data.GetBootImage(self._out_dir, self._GetTargetSdkArch(),
                                   boot_data.TARGET_TYPE_GENERIC))
    ]

    if self._node_name:
      bootserver_command += ['-n', self._node_name]

    bootserver_command += ['--']
    bootserver_command += boot_data.GetKernelArgs(self._out_dir)

    logging.debug(' '.join(bootserver_command))
    _, stdout = SubprocessCallWithTimeout(bootserver_command,
                                          silent=False,
                                          timeout_secs=300)

    self._ParseNodename(stdout)

    # Start loglistener to save system logs.
    if self._system_log_file:
      self._StartLoglistener()

    # Repeatdly query mDNS until we find the device, or we hit
    # BOOT_DISCOVERY_ATTEMPTS
    logging.info('Waiting for device to join network.')
    for _ in xrange(device_target.BOOT_DISCOVERY_ATTEMPTS):
      if self.__Discover():
        break

    if not self._host:
      raise Exception("Device %s couldn't be discovered via mDNS." %
                      self._node_name)

    self._WaitUntilReady()

    # Update the target's hash to match the current tree's.
    self.PutFile(os.path.join(SDK_ROOT, '.hash'), TARGET_HASH_FILE_PATH)

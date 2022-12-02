# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running/interacting with Fuchsia on an emulator."""

import json
import logging
import os
import subprocess
import sys
import tempfile

import boot_data
import common
import ffx_session
import pkg_repo
import target

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from compatible_utils import find_image_in_sdk, running_unattended


class EmuTarget(target.Target):
  LOCAL_ADDRESS = 'localhost'

  def __init__(self, out_dir, target_cpu, logs_dir, image):
    """out_dir: The directory which will contain the files that are
                   generated to support the emulator deployment.
    target_cpu: The emulated target CPU architecture.
                Can be 'x64' or 'arm64'."""

    super(EmuTarget, self).__init__(out_dir, target_cpu, logs_dir)
    self._emu_process = None
    self._pkg_repo = None
    self._target_context = None
    self._ffx_target = None

    self._pb_path = self._GetPbPath(image)
    metadata = self._GetEmuMetadata()
    self._disk_image = metadata['disk_images'][0]
    self._kernel = metadata['kernel']
    self._ramdisk = metadata['initial_ramdisk']

  def _GetPbPath(self, image):
    if not image:
      image = 'terminal.qemu-%s' % self._target_cpu
    image_path = find_image_in_sdk(image,
                                   product_bundle=True,
                                   sdk_root=os.path.dirname(common.IMAGES_ROOT))
    if not image_path:
      raise FileNotFoundError(f'Product bundle {image} is not downloaded. Add '
                              'the image and run "gclient sync" again.')
    return image_path

  def _GetEmuMetadata(self):
    with open(os.path.join(self._pb_path, 'product_bundle.json')) as f:
      return json.load(f)['data']['manifests']['emu']

  def __enter__(self):
    return self

  def _BuildCommand(self):
    """Build the command that will be run to start Fuchsia in the emulator."""
    pass

  def _SetEnv(self):
    return os.environ.copy()

  def Start(self):
    if running_unattended() and not self._HasNetworking():
      # Bots may accumulate stale manually-added targets with the same address
      # as the one to be added here. Preemtively remove any unknown targets at
      # this address before starting the emulator and adding it as a target.
      self._ffx_runner.remove_stale_targets('127.0.0.1')
    emu_command = self._BuildCommand()
    logging.debug(' '.join(emu_command))

    # Zircon sends debug logs to serial port (see kernel.serial=legacy flag
    # above). Serial port is redirected to a file through emulator stdout.
    # If runner_logs are not enabled, we output the kernel serial log
    # to a temporary file, and print that out if we are unable to connect to
    # the emulator guest, to make it easier to diagnose connectivity issues.
    temporary_log_file = None
    if self._log_manager.IsLoggingEnabled():
      stdout = self._log_manager.Open('serial_log')
    else:
      temporary_log_file = tempfile.NamedTemporaryFile('w')
      stdout = temporary_log_file

    self.LogProcessStatistics('proc_stat_start_log')
    self.LogSystemStatistics('system_statistics_start_log')

    self._emu_process = subprocess.Popen(emu_command,
                                         stdin=open(os.devnull),
                                         stdout=stdout,
                                         stderr=subprocess.STDOUT,
                                         env=self._SetEnv())
    try:
      self._ConnectToTarget()
      self.LogProcessStatistics('proc_stat_ready_log')
    except target.FuchsiaTargetException:
      self._DisconnectFromTarget()
      if temporary_log_file:
        logging.info('Kernel logs:\n' +
                     open(temporary_log_file.name, 'r').read())
      raise

  def GetFfxTarget(self):
    assert self._ffx_target
    return self._ffx_target

  def Stop(self):
    try:
      self._DisconnectFromTarget()
      self._Shutdown()
    finally:
      self.LogProcessStatistics('proc_stat_end_log')
      self.LogSystemStatistics('system_statistics_end_log')
      super(EmuTarget, self).Stop()

  def GetPkgRepo(self):
    if not self._pkg_repo:
      self._pkg_repo = pkg_repo.ManagedPkgRepo(self)

    return self._pkg_repo

  def _Shutdown(self):
    """Shuts down the emulator."""
    raise NotImplementedError()

  def _HasNetworking(self):
    """Returns `True` if the emulator will be started with networking (e.g.,
    TUN/TAP emulated networking).
    """
    raise NotImplementedError()

  def _IsEmuStillRunning(self):
    """Returns `True` if the emulator is still running."""
    raise NotImplementedError()

  def _GetEndpoint(self):
    raise NotImplementedError()

  def _ConnectToTarget(self):
    with_network = self._HasNetworking()
    if not with_network:
      # The target was started without networking, so tell ffx how to find it.
      logging.info('Connecting to Fuchsia using ffx.')
      _, host_ssh_port = self._GetEndpoint()
      self._target_context = self._ffx_runner.scoped_target_context(
          '127.0.0.1', host_ssh_port)
      self._ffx_target = self._target_context.__enter__()
      self._ffx_target.wait(common.ATTACH_RETRY_SECONDS)
    super(EmuTarget, self)._ConnectToTarget()
    if with_network:
      # Interact with the target via its address:port, which ffx should now know
      # about.
      self._ffx_target = ffx_session.FfxTarget.from_address(
          self._ffx_runner, *self._GetEndpoint())

  def _DisconnectFromTarget(self):
    self._ffx_target = None
    if self._target_context:
      self._target_context.__exit__(None, None, None)
      self._target_context = None
    super(EmuTarget, self)._DisconnectFromTarget()

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath()

  def LogSystemStatistics(self, log_file_name):
    self._LaunchSubprocessWithLogs(['top', '-b', '-n', '1'], log_file_name)
    self._LaunchSubprocessWithLogs(['ps', '-ax'], log_file_name)

  def LogProcessStatistics(self, log_file_name):
    self._LaunchSubprocessWithLogs(['cat', '/proc/stat'], log_file_name)

  def _LaunchSubprocessWithLogs(self, command, log_file_name):
    """Launch a subprocess and redirect stdout and stderr to log_file_name.
    Command will not be run if logging directory is not set."""

    if not self._log_manager.IsLoggingEnabled():
      return
    log = self._log_manager.Open(log_file_name)
    subprocess.call(command,
                    stdin=open(os.devnull),
                    stdout=log,
                    stderr=subprocess.STDOUT)

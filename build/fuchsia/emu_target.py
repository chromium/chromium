# Copyright 2019 The Chromium Authors. All rights reserved.
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
import pkg_repo
import target


class EmuTarget(target.Target):
  LOCAL_ADDRESS = '127.0.0.1'

  def __init__(self, out_dir, target_cpu, logs_dir):
    """out_dir: The directory which will contain the files that are
                   generated to support the emulator deployment.
    target_cpu: The emulated target CPU architecture.
                Can be 'x64' or 'arm64'."""

    super(EmuTarget, self).__init__(out_dir, target_cpu, logs_dir)
    self._emu_process = None
    self._pkg_repo = None
    self._target_added = False

  def __enter__(self):
    return self

  def _BuildCommand(self):
    """Build the command that will be run to start Fuchsia in the emulator."""
    pass

  def _SetEnv(self):
    return os.environ.copy()

  def Start(self):
    if common.IsRunningUnattended():
      # On the bots, we sometimes find that a previous ffx daemon instance is
      # wedged, leading to failures. Reach out and stop an old daemon if there
      # happens to be one.
      self._StopFfxDaemon()
      # Bots may accumulate stale manually-added targets with the same address
      # as the one to be added here. Preemtively remove any unknown targets at
      # this address before starting the emulator and adding it as a target.
      self._RemoveStaleTargets(self.LOCAL_ADDRESS)
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
      self._WaitUntilReady()
      self.LogProcessStatistics('proc_stat_ready_log')
      self._AddFfxTarget()
    except target.FuchsiaTargetException:
      if temporary_log_file:
        logging.info('Kernel logs:\n' +
                     open(temporary_log_file.name, 'r').read())
      raise

  def Stop(self):
    try:
      super(EmuTarget, self).Stop()
    finally:
      self.Shutdown()

  def GetPkgRepo(self):
    if not self._pkg_repo:
      self._pkg_repo = pkg_repo.ManagedPkgRepo(self)

    return self._pkg_repo

  def Shutdown(self):
    if not self._emu_process:
      logging.error('%s did not start' % (self.EMULATOR_NAME))
      return
    self._RemoveFfxTarget()
    if common.IsRunningUnattended():
      self._StopFfxDaemon()
    returncode = self._emu_process.poll()
    if returncode == None:
      logging.info('Shutting down %s' % (self.EMULATOR_NAME))
      self._emu_process.kill()
    elif returncode == 0:
      logging.info('%s quit unexpectedly without errors' % self.EMULATOR_NAME)
    elif returncode < 0:
      logging.error('%s was terminated by signal %d' %
                    (self.EMULATOR_NAME, -returncode))
    else:
      logging.error('%s quit unexpectedly with exit code %d' %
                    (self.EMULATOR_NAME, returncode))

    self.LogProcessStatistics('proc_stat_end_log')
    self.LogSystemStatistics('system_statistics_end_log')


  def _IsEmuStillRunning(self):
    if not self._emu_process:
      return False
    return os.waitpid(self._emu_process.pid, os.WNOHANG)[0] == 0

  def _GetEndpoint(self):
    if not self._IsEmuStillRunning():
      raise Exception('%s quit unexpectedly.' % (self.EMULATOR_NAME))
    return (self.LOCAL_ADDRESS, self._host_ssh_port)

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath()

  def _RunFfxCommand(self, arguments):
    """Runs an ffx command with some arguments.

    Args:
      arguments: An iterable of the argument strings to pass to ffx.

    Returns:
      A string holding merged stdout and stderr from the command if the command
      succeeded.

    Raises:
      Any exception raised by subprocess.run().
    """
    ffx = os.path.join(common.SDK_ROOT, 'tools',
                       common.GetHostArchFromPlatform(), 'ffx')
    command = [ffx]
    command.extend(arguments)
    logging.debug(command)
    try:
      completed_process = subprocess.run(command,
                                         check=True,
                                         stdin=open(os.devnull, 'r'),
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT,
                                         encoding='utf-8')
      if completed_process.stdout:
        logging.debug('ffx succeeded with output\n%s' %
                      completed_process.stdout)
      return completed_process.stdout
    except subprocess.CalledProcessError as cpe:
      logging.exception('ffx failed with output\n%s' % cpe.stdout)
      raise

  def _StopFfxDaemon(self):
    try:
      self._RunFfxCommand(['daemon', 'stop'])
      return
    except subprocess.CalledProcessError as cpe:
      pass
    logging.error('Failed to stop the damon. Attempting to restart it via ffx' +
                  ' doctor')
    self._RunFfxCommand(['doctor', '--restart-daemon'])

  def _RemoveStaleTargets(self, address):
    """Removes any targets from ffx that are listening at a given address.

    Args:
      address: A string representation of the target's ip address.
    """
    json_targets = self._RunFfxCommand(['target', 'list', '-f', 'j'])
    if not json_targets:
      return
    try:
      targets = json.loads(json_targets)
    except json.JSONDecodeError as e:
      logging.debug('ffx target list returned non-json text: %s' % json_targets)
      return
    for target in targets:
      if target['rcs_state'] == 'N' and address in target['addresses']:
        self._RunFfxCommand(['target', 'remove', address])

  def _SetDefaultFfxTarget(self, address):
    """Sets the default target for other ffx commands.

    Args:
      address: A string representation of the target's ip address.
    """
    targets = json.loads(self._RunFfxCommand(['target', 'list', '-f', 'j']))
    for target in targets:
      if (target['rcs_state'] == 'N' or target['nodename'] == '<unknown>'
          or address not in target['addresses']):
        continue
      self._RunFfxCommand(['target', 'default', 'set', target['nodename']])
      break

  def _AddFfxTarget(self):
    """Adds a target to ffx for the emulated target."""
    endpoint = self._GetEndpoint()
    self._RunFfxCommand(['target', 'add', ':'.join(map(str, endpoint))])
    self._target_added = True
    self._SetDefaultFfxTarget(endpoint[0])

  def _RemoveFfxTarget(self):
    """Removes the emulated target from ffx."""
    if not self._target_added:
      return
    self._RunFfxCommand(
        ['target', 'remove', ':'.join(map(str, self._GetEndpoint()))])
    self._target_added = False

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

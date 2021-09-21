# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running/interacting with Fuchsia on an emulator."""

import pkg_repo
import boot_data
import logging
import os
import subprocess
import sys
import target
import tempfile


class EmuTarget(target.Target):
  def __init__(self, out_dir, target_cpu, logs_dir):
    """out_dir: The directory which will contain the files that are
                   generated to support the emulator deployment.
    target_cpu: The emulated target CPU architecture.
                Can be 'x64' or 'arm64'."""

    super(EmuTarget, self).__init__(out_dir, target_cpu, logs_dir)
    self._emu_process = None
    self._pkg_repo = None

  def __enter__(self):
    return self

  def _BuildCommand(self):
    """Build the command that will be run to start Fuchsia in the emulator."""
    pass

  def _SetEnv(self):
    return os.environ.copy()

  def Start(self):
    emu_command = self._BuildCommand()

    # We pass a separate stdin stream. Sharing stdin across processes
    # leads to flakiness due to the OS prematurely killing the stream and the
    # Python script panicking and aborting.
    # The precise root cause is still nebulous, but this fix works.
    # See crbug.com/741194.
    logging.debug('Launching %s.' % (self.EMULATOR_NAME))
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
    return ('localhost', self._host_ssh_port)

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

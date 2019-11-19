# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running/interacting with Fuchsia on an emulator."""

import amber_repo
import boot_data
import logging
import os
import subprocess
import sys
import target
import tempfile

class EmuTarget(target.Target):
  def __init__(self, output_dir, target_cpu, system_log_file):
    """output_dir: The directory which will contain the files that are
                   generated to support the emulator deployment.
    target_cpu: The emulated target CPU architecture.
                Can be 'x64' or 'arm64'."""
    super(EmuTarget, self).__init__(output_dir, target_cpu)
    self._emu_process = None
    self._system_log_file = system_log_file

  def __enter__(self):
    return self

  def _GetEmulatorName(self):
    pass

  def _BuildCommand(self):
    """Build the command that will be run to start Fuchsia in the emulator."""
    pass

  # Used by the context manager to ensure that the emulator is killed when
  # the Python process exits.
  def __exit__(self, exc_type, exc_val, exc_tb):
    self.Shutdown();

  def Start(self):
    emu_command = self._BuildCommand()

    # We pass a separate stdin stream. Sharing stdin across processes
    # leads to flakiness due to the OS prematurely killing the stream and the
    # Python script panicking and aborting.
    # The precise root cause is still nebulous, but this fix works.
    # See crbug.com/741194.
    logging.debug('Launching %s.' % (self._GetEmulatorName()))
    logging.debug(' '.join(emu_command))

    # Zircon sends debug logs to serial port (see kernel.serial=legacy flag
    # above). Serial port is redirected to a file through emulator stdout.
    # Unless a |_system_log_file| is explicitly set, we output the kernel serial
    # log to a temporary file, and print that out if we are unable to connect to
    # the emulator guest, to make it easier to diagnose connectivity issues.
    temporary_system_log_file = None
    if self._system_log_file:
      stdout = self._system_log_file
      stderr = subprocess.STDOUT
    else:
      temporary_system_log_file = tempfile.NamedTemporaryFile('w')
      stdout = temporary_system_log_file
      stderr = sys.stderr

    self._emu_process = subprocess.Popen(emu_command, stdin=open(os.devnull),
                                          stdout=stdout, stderr=stderr)

    try:
      self._WaitUntilReady();
    except target.FuchsiaTargetException:
      if temporary_system_log_file:
        logging.info('Kernel logs:\n' +
                     open(temporary_system_log_file.name, 'r').read())
      raise

  def _GetAmberRepo(self):
    return amber_repo.ManagedAmberRepo(self)

  def Shutdown(self):
    if self._IsEmuStillRunning():
      logging.info('Shutting down %s' % (self._GetEmulatorName()))
      self._emu_process.kill()

  def _IsEmuStillRunning(self):
    if not self._emu_process:
      return False
    return os.waitpid(self._emu_process.pid, os.WNOHANG)[0] == 0

  def _GetEndpoint(self):
    if not self._IsEmuStillRunning():
      raise Exception('%s quit unexpectedly.' % (self._GetEmulatorName()))
    return ('localhost', self._host_ssh_port)

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath(self._output_dir)

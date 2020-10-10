# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running/interacting with Fuchsia on an emulator."""

import amber_repo
import boot_data
import logging
import os
import runner_logs
import subprocess
import sys
import target
import tempfile


class EmuTarget(target.Target):
  def __init__(self, out_dir, target_cpu, system_log_file, fuchsia_out_dir):
    """out_dir: The directory which will contain the files that are
                   generated to support the emulator deployment.
    target_cpu: The emulated target CPU architecture.
                Can be 'x64' or 'arm64'."""

    # fuchsia_out_dir is unused by emulator targets.
    del fuchsia_out_dir

    super(EmuTarget, self).__init__(out_dir, target_cpu)
    self._emu_process = None
    self._system_log_file = system_log_file
    self._amber_repo = None

  @staticmethod
  def RegisterArgs(arg_parser):
    target.Target.RegisterArgs(arg_parser)
    emu_args = arg_parser.add_argument_group('emu', 'Emulator arguments')
    emu_args.add_argument('--cpu-cores',
                          type=int,
                          default=4,
                          help='Sets the number of CPU cores to provide.')
    emu_args.add_argument('--ram-size-mb',
                          type=int,
                          default=2048,
                          help='Sets the RAM size (MB) if launching in a VM'),
    emu_args.add_argument('--allow-no-kvm',
                          action='store_false',
                          dest='require_kvm',
                          default=True,
                          help='Do not require KVM acceleration for '
                          'emulators.')

  def __enter__(self):
    return self

  def _BuildCommand(self):
    """Build the command that will be run to start Fuchsia in the emulator."""
    pass

  def _SetEnv(self):
    return os.environ.copy()

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
    logging.debug('Launching %s.' % (self.EMULATOR_NAME))
    logging.debug(' '.join(emu_command))

    # Zircon sends debug logs to serial port (see kernel.serial=legacy flag
    # above). Serial port is redirected to a file through emulator stdout.
    # If runner_logs are not enabled, we output the kernel serial log
    # to a temporary file, and print that out if we are unable to connect to
    # the emulator guest, to make it easier to diagnose connectivity issues.
    temporary_log_file = None
    if runner_logs.IsEnabled():
      stdout = runner_logs.FileStreamFor('serial_log')
    else:
      temporary_log_file = tempfile.NamedTemporaryFile('w')
      stdout = temporary_log_file

    # TODO(crbug.com/1100402): Delete when no longer needed for debug info.
    # Log system statistics at the start of the emulator run.
    _LogSystemStatistics('system_start_statistics_log')

    self._emu_process = subprocess.Popen(emu_command,
                                         stdin=open(os.devnull),
                                         stdout=stdout,
                                         stderr=subprocess.STDOUT,
                                         env=self._SetEnv())

    try:
      self._WaitUntilReady()
    except target.FuchsiaTargetException:
      if temporary_log_file:
        logging.info('Kernel logs:\n' +
                     open(temporary_log_file.name, 'r').read())
      raise

  def GetAmberRepo(self):
    if not self._amber_repo:
      self._amber_repo = amber_repo.ManagedAmberRepo(self)

    return self._amber_repo

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

    # TODO(crbug.com/1100402): Delete when no longer needed for debug info.
    # Log system statistics at the end of the emulator run.
    _LogSystemStatistics('system_end_statistics_log')


  def _IsEmuStillRunning(self):
    if not self._emu_process:
      return False
    return os.waitpid(self._emu_process.pid, os.WNOHANG)[0] == 0

  def _GetEndpoint(self):
    if not self._IsEmuStillRunning():
      raise Exception('%s quit unexpectedly.' % (self.EMULATOR_NAME))
    return ('localhost', self._host_ssh_port)

  def _GetSshConfigPath(self):
    return boot_data.GetSSHConfigPath(self._out_dir)


# TODO(crbug.com/1100402): Delete when no longer needed for debug info.
def _LogSystemStatistics(log_file_name):
  statistics_log = runner_logs.FileStreamFor(log_file_name)
  # Log the cpu load and process information.
  subprocess.call(['top', '-b', '-n', '1'],
                  stdin=open(os.devnull),
                  stdout=statistics_log,
                  stderr=subprocess.STDOUT)
  subprocess.call(['ps', '-ax'],
                  stdin=open(os.devnull),
                  stdout=statistics_log,
                  stderr=subprocess.STDOUT)

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import threading

_SSH = ['ssh']
_SCP = ['scp', '-C']  # Use gzip compression.
_SSH_LOGGER = logging.getLogger('ssh')

COPY_TO_TARGET = 0
COPY_FROM_TARGET = 1


def _IsLinkLocalIPv6(hostname):
  return hostname.startswith('fe80::')

def _EscapeIfIPv6Address(address):
  if ':' in address:
    return '[' + address + ']'
  else:
    return address

class CommandRunner(object):
  """Helper class used to execute commands on a remote host over SSH."""

  def __init__(self, config_path, host, port):
    """Creates a CommandRunner that connects to the specified |host| and |port|
    using the ssh config at the specified |config_path|.

    config_path: Full path to SSH configuration.
    host: The hostname or IP address of the remote host.
    port: The port to connect to."""

    self._config_path = config_path
    self._host = host
    self._port = port

  def _GetSshCommandLinePrefix(self):
    return _SSH + ['-F', self._config_path, self._host, '-p', str(self._port)]

  def RunCommand(self, command, silent, timeout_secs=None):
    """Executes an SSH command on the remote host and blocks until completion.

    command: A list of strings containing the command and its arguments.
    silent: If true, suppresses all output from 'ssh'.
    timeout_secs: If set, limits the amount of time that |command| may run.
                  Commands which exceed the timeout are killed.

    Returns the exit code from the remote command."""

    ssh_command = self._GetSshCommandLinePrefix() + command
    _SSH_LOGGER.debug('ssh exec: ' + ' '.join(ssh_command))
    if silent:
      devnull = open(os.devnull, 'w')
      process = subprocess.Popen(ssh_command, stderr=devnull, stdout=devnull)
    else:
      process = subprocess.Popen(ssh_command)

    timeout_timer = None
    if timeout_secs:
      timeout_timer = threading.Timer(timeout_secs, process.kill)
      timeout_timer.start()

    process.wait()

    if timeout_timer:
      timeout_timer.cancel()

    if process.returncode == -9:
      raise Exception('Timeout when executing \"%s\".' % ' '.join(command))

    return process.returncode


  def RunCommandPiped(self, command = None, ssh_args = None, **kwargs):
    """Executes an SSH command on the remote host and returns a process object
    with access to the command's stdio streams. Does not block.

    command: A list of strings containing the command and its arguments.
    ssh_args: Arguments that will be passed to SSH.
    kwargs: A dictionary of parameters to be passed to subprocess.Popen().
            The parameters can be used to override stdin and stdout, for
            example.

    Returns a Popen object for the command."""

    if not command:
      command = []
    if not ssh_args:
      ssh_args = []

    ssh_command = self._GetSshCommandLinePrefix() + ssh_args + ['--'] + command
    _SSH_LOGGER.debug(' '.join(ssh_command))
    return subprocess.Popen(ssh_command, **kwargs)


  def RunScp(self, sources, dest, direction, recursive=False):
    """Copies a file to or from a remote host using SCP and blocks until
    completion.

    sources: Paths of the files to be copied.
    dest: The path that |source| will be copied to.
    direction: Indicates whether the file should be copied to
               or from the remote side.
               Valid values are COPY_TO_TARGET or COPY_FROM_TARGET.
    recursive: If true, performs a recursive copy.

    Function will raise an assertion if a failure occurred."""

    scp_command = _SCP[:]
    if _SSH_LOGGER.getEffectiveLevel() == logging.DEBUG:
      scp_command.append('-v')
    if recursive:
      scp_command.append('-r')

    host = _EscapeIfIPv6Address(self._host)

    if direction == COPY_TO_TARGET:
      dest = "%s:%s" % (host, dest)
    else:
      sources = ["%s:%s" % (host, source) for source in sources]

    scp_command += ['-F', self._config_path, '-P', str(self._port)]
    scp_command += sources
    scp_command += [dest]

    _SSH_LOGGER.debug(' '.join(scp_command))
    subprocess.check_call(scp_command, stdout=open(os.devnull, 'w'))

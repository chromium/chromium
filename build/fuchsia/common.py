# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import platform
import shutil
import socket
import subprocess
import sys

DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
IMAGES_ROOT = os.path.join(
    DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk', 'images')
SDK_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk', 'sdk')

# The number of seconds to wait when trying to attach to a target.
ATTACH_RETRY_SECONDS = 120


def EnsurePathExists(path):
  """Checks that the file |path| exists on the filesystem and returns the path
  if it does, raising an exception otherwise."""

  if not os.path.exists(path):
    raise IOError('Missing file: ' + path)

  return path

def GetHostOsFromPlatform():
  host_platform = sys.platform
  if host_platform.startswith('linux'):
    return 'linux'
  elif host_platform.startswith('darwin'):
    return 'mac'
  raise Exception('Unsupported host platform: %s' % host_platform)

def GetHostArchFromPlatform():
  host_arch = platform.machine()
  # platform.machine() returns AMD64 on 64-bit Windows.
  if host_arch in ['x86_64', 'AMD64']:
    return 'x64'
  elif host_arch == 'aarch64':
    return 'arm64'
  raise Exception('Unsupported host architecture: %s' % host_arch)

def GetHostToolPathFromPlatform(tool):
  host_arch = platform.machine()
  return os.path.join(SDK_ROOT, 'tools', GetHostArchFromPlatform(), tool)


# Remove when arm64 emulator is also included in Fuchsia SDK.
def GetEmuRootForPlatform(emulator):
  if GetHostArchFromPlatform() == 'x64':
    return GetHostToolPathFromPlatform('{0}_internal'.format(emulator))
  return os.path.join(
      DIR_SOURCE_ROOT, 'third_party', '{0}-{1}-{2}'.format(
          emulator, GetHostOsFromPlatform(), GetHostArchFromPlatform()))


def ConnectPortForwardingTask(target, local_port, remote_port = 0):
  """Establishes a port forwarding SSH task to a localhost TCP endpoint hosted
  at port |local_port|. Blocks until port forwarding is established.

  Returns the remote port number."""

  forwarding_flags = ['-O', 'forward',  # Send SSH mux control signal.
                      '-R', '%d:localhost:%d' % (remote_port, local_port),
                      '-v',   # Get forwarded port info from stderr.
                      '-NT']  # Don't execute command; don't allocate terminal.

  if remote_port != 0:
    # Forward to a known remote port.
    task = target.RunCommand([], ssh_args=forwarding_flags)
    if task.returncode != 0:
      raise Exception('Could not establish a port forwarding connection.')
    return

  task = target.RunCommandPiped([],
                                ssh_args=forwarding_flags,
                                stdout=subprocess.PIPE,
                                stderr=open('/dev/null'))
  output = task.stdout.readlines()
  task.wait()
  if task.returncode != 0:
    raise Exception('Got an error code when requesting port forwarding: %d' %
                    task.returncode)

  parsed_port = int(output[0].strip())
  logging.debug('Port forwarding established (local=%d, device=%d)' %
                (local_port, parsed_port))
  return parsed_port


def GetAvailableTcpPort():
  """Finds a (probably) open port by opening and closing a listen socket."""
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.bind(("", 0))
  port = sock.getsockname()[1]
  sock.close()
  return port


def RunGnSdkFunction(script, function):
  script_path = os.path.join(SDK_ROOT, 'bin', script)
  function_cmd = ['bash', '-c', '. %s; %s' % (script_path, function)]
  return SubprocessCallWithTimeout(function_cmd)


def SubprocessCallWithTimeout(command, timeout_secs=None):
  """Helper function for running a command.

  Args:
    command: The command to run.
    timeout_secs: Maximum amount of time allowed for the command to finish.

  Returns:
    A tuple of (return code, stdout, stderr) of the command. Raises
    an exception if the subprocess times out.
  """

  process = None
  try:
    process = subprocess.run(command,
                             capture_output=True,
                             timeout=timeout_secs,
                             encoding='utf-8')
  except subprocess.TimeoutExpired as te:
    raise TimeoutError(str(te))

  return process.returncode, process.stdout, process.stderr


def IsRunningUnattended():
  """Returns true if running non-interactively.

  When running unattended, confirmation prompts and the like are suppressed.
  """
  # Chromium tests only for the presence of the variable, so match that here.
  return 'CHROME_HEADLESS' in os.environ


def MakeCleanDirectory(directory_name):
  """If the directory exists, delete it and then remake it with no contents."""
  if os.path.exists(directory_name):
    shutil.rmtree(directory_name)
  os.mkdir(directory_name)

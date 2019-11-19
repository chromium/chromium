# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import platform
import socket
import subprocess
import sys

DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
SDK_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk', 'sdk')
IMAGES_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk',
                           'images')
ARM64_SDK_TOOLS = os.path.join(DIR_SOURCE_ROOT, 'third_party',
                               'fuchsia-sdk-arm64', 'tools')
X64_SDK_TOOLS = os.path.join(SDK_ROOT, 'tools')

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
  if host_arch == 'x86_64':
    return 'x64'
  elif host_arch == 'aarch64':
    return 'arm64'
  raise Exception('Unsupported host architecture: %s' % host_arch)

def GetHostToolPathFromPlatform(tool):
  host_arch = platform.machine()
  if host_arch == 'x86_64':
    return os.path.join(X64_SDK_TOOLS, tool)
  elif host_arch == 'aarch64':
    return os.path.join(ARM64_SDK_TOOLS, tool)
  raise Exception('Unsupported host architecture: %s' % host_arch)

def GetEmuRootForPlatform(emulator):
  return os.path.join(DIR_SOURCE_ROOT, 'third_party',
                      emulator + '-' + GetHostOsFromPlatform() + '-' +
                       GetHostArchFromPlatform())

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

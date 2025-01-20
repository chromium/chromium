# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import os
import pathlib
import socket
import platform
import sys
import struct

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
from util import build_utils

# Use a unix abstract domain socket:
# https://man7.org/linux/man-pages/man7/unix.7.html#:~:text=abstract:
SOCKET_ADDRESS = '\0chromium_build_server_socket'
BUILD_SERVER_ENV_VARIABLE = 'INVOKED_BY_BUILD_SERVER'

ADD_TASK = 'add_task'
QUERY_BUILD = 'query_build'
POLL_HEARTBEAT = 'poll_heartbeat'
REGISTER_BUILDER = 'register_builder'
CANCEL_BUILD = 'cancel_build'

SERVER_SCRIPT = pathlib.Path(
    build_utils.DIR_SOURCE_ROOT
) / 'build' / 'android' / 'fast_local_dev_server.py'


def MaybeRunCommand(name, argv, stamp_file, use_build_server=False):
  """Returns True if the command was successfully sent to the build server."""

  if platform.system() == "Darwin":
    # Build server does not support Mac.
    return False

  # When the build server runs a command, it sets this environment variable.
  # This prevents infinite recursion where the script sends a request to the
  # build server, then the build server runs the script, and then the script
  # sends another request to the build server.
  if BUILD_SERVER_ENV_VARIABLE in os.environ:
    return False

  if not use_build_server:
    return False

  autoninja_tty = os.environ.get('AUTONINJA_STDOUT_NAME')
  autoninja_build_id = os.environ.get('AUTONINJA_BUILD_ID')

  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    try:
      sock.connect(SOCKET_ADDRESS)
    except socket.error as e:
      # [Errno 111] Connection refused. Either the server has not been started
      #             or the server is not currently accepting new connections.
      if e.errno == 111:
        raise RuntimeError(
            '\n\nBuild server is not running and '
            'android_static_analysis="build_server" is set.\n\n') from None
      raise e

    SendMessage(
        sock,
        json.dumps({
            'name': name,
            'message_type': ADD_TASK,
            'cmd': argv,
            'cwd': os.getcwd(),
            'tty': autoninja_tty,
            'build_id': autoninja_build_id,
            'stamp_file': stamp_file,
        }).encode('utf8'))

  # Siso needs the stamp file to be created in order for the build step to
  # complete. If the task fails when the build server runs it, the build server
  # will delete the stamp file so that it will be run again next build.
  build_utils.Touch(stamp_file)
  return True


def MaybeTouch(stamp_file):
  """Touch |stamp_file| if we are not running under the build_server."""
  # If we are running under the build server, the stamp file has already been
  # touched when the task was created. If we touch it again, siso will consider
  # the target dirty.
  if BUILD_SERVER_ENV_VARIABLE in os.environ:
    return
  build_utils.Touch(stamp_file)


def SendMessage(sock: socket.socket, message: bytes):
  size_prefix = struct.pack('!i', len(message))
  sock.sendall(size_prefix + message)


def ReceiveMessage(sock: socket.socket):
  size_prefix = b''
  remaining = 4  # sizeof(int)
  while remaining > 0:
    data = sock.recv(remaining)
    if not data:
      return None
    remaining -= len(data)
    size_prefix += data
  remaining, = struct.unpack('!i', size_prefix)
  received = []
  while remaining > 0:
    data = sock.recv(remaining)
    if not data:
      break
    received.append(data)
    remaining -= len(data)
  return b''.join(received)

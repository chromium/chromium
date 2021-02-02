# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import os
import pipes
import socket
import tempfile

# Use a unix abstract domain socket:
# https://man7.org/linux/man-pages/man7/unix.7.html#:~:text=abstract:
SOCKET_ADDRESS = '\0chromium_build_server_socket'
BUILD_SERVER_ENV_VARIABLE = 'INVOKED_BY_BUILD_SERVER'


def MaybeRunCommand(name, cmd):
  """Returns True if the command was successfully sent to the build server."""

  # When the build server runs a command, it sets this environment variable.
  # This prevents infinite recursion where the script sends a request to the
  # build server, then the build server runs the script, and then the script
  # sends another request to the build server.
  if BUILD_SERVER_ENV_VARIABLE in os.environ:
    return False
  with contextlib.closing(socket.socket(socket.AF_UNIX)) as sock:
    try:
      sock.connect(SOCKET_ADDRESS)
      sock.sendall(json.dumps({
          'name': name,
          'cmd': cmd,
          'cwd': os.getcwd(),
      }))
    except socket.error as e:
      if e.errno == 111:  # [Errno 111] Connection refused
        return False
      raise e
  return True

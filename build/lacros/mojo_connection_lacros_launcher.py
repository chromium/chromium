#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helps launch lacros-chrome with mojo connection established on Linux
  or Chrome OS. Use on Chrome OS is for dev purposes.

  The main use case is to be able to launch lacros-chrome in a debugger.

  Please first launch an ash-chrome in the background as usual except without
  the '--lacros-chrome-path' argument and with an additional
  '--lacros-mojo-socket-for-testing' argument pointing to a socket path:

  XDG_RUNTIME_DIR=/tmp/ash_chrome_xdg_runtime ./out/ash/chrome \\
      --user-data-dir=/tmp/ash-chrome --enable-wayland-server \\
      --no-startup-window --enable-features=LacrosSupport \\
      --lacros-mojo-socket-for-testing=/tmp/lacros.sock

  Then, run this script with '-s' pointing to the same socket path used to
  launch ash-chrome, followed by a command one would use to launch lacros-chrome
  inside a debugger:

  EGL_PLATFORM=surfaceless XDG_RUNTIME_DIR=/tmp/ash_chrome_xdg_runtime \\
  ./build/lacros/mojo_connection_lacros_launcher.py -s /tmp/lacros.sock
  gdb --args ./out/lacros-release/chrome --user-data-dir=/tmp/lacros-chrome
"""

import argparse
import array
import contextlib
import os
import pathlib
import socket
import sys
import subprocess


def _ReceiveFD(sock):
  """Receives a FD from ash-chrome that will be used to launch lacros-chrome.

  Args:
    sock: A connected unix domain socket.

  Returns:
    An integer represeting the received file descriptor.
  """
  # This function is borrowed from with modifications:
  # https://docs.python.org/3/library/socket.html#socket.socket.recvmsg
  fds = array.array("i")  # Array of ints
  # Along with the file descriptor, ash-chrome also sends the version in the
  # regular data.
  version, ancdata, _, _ = sock.recvmsg(1, socket.CMSG_LEN(fds.itemsize))
  for cmsg_level, cmsg_type, cmsg_data in ancdata:
    if cmsg_level == socket.SOL_SOCKET and cmsg_type == socket.SCM_RIGHTS:
      assert len(cmsg_data) == fds.itemsize, 'Expecting exactly 1 FD'
      fds.frombytes(cmsg_data[:fds.itemsize])

  assert version == b'\x00', 'Expecting version code to be 0'
  assert len(list(fds)) == 1, 'Expecting exactly 1 FD'
  return os.fdopen(list(fds)[0])


def Main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__
  arg_parser.add_argument(
      '-s',
      '--socket-path',
      type=pathlib.Path,
      required=True,
      help='Absolute path to the socket that were used to start ash-chrome, '
      'for example: "/tmp/lacros.socket"')
  args = arg_parser.parse_known_args()

  assert 'XDG_RUNTIME_DIR' in os.environ
  assert os.environ.get('EGL_PLATFORM') == 'surfaceless'

  with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.connect(args[0].socket_path.as_posix())
    file_obj = _ReceiveFD(sock)
    assert file_obj, ('Failed to connect to the socket: %s' %
                      args[0].socket_path.as_posix())

  with contextlib.closing(file_obj):
    cmd = args[1] + ['--mojo-platform-channel-handle=%d' % file_obj.fileno()]
    proc = subprocess.Popen(cmd, pass_fds=(file_obj.fileno(), ))

  return proc.wait()


if __name__ == '__main__':
  sys.exit(Main())

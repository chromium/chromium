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


_NUM_FDS_MAX = 3


# contextlib.nullcontext is introduced in 3.7, while Python version on
# CrOS is still 3.6. This is for backward compatibility.
class NullContext:
  def __init__(self, enter_ret=None):
    self.enter_ret = enter_ret

  def __enter__(self):
    return self.enter_ret

  def __exit__(self, exc_type, exc_value, trace):
    pass


def _ReceiveFDs(sock):
  """Receives FDs from ash-chrome that will be used to launch lacros-chrome.

  Args:
    sock: A connected unix domain socket.

  Returns:
    File objects for the mojo connection and maybe startup data file.
  """
  # This function is borrowed from with modifications:
  # https://docs.python.org/3/library/socket.html#socket.socket.recvmsg
  fds = array.array("i")  # Array of ints
  # Along with the file descriptor, ash-chrome also sends the version in the
  # regular data.
  version, ancdata, _, _ = sock.recvmsg(
      1, socket.CMSG_LEN(fds.itemsize * _NUM_FDS_MAX))
  for cmsg_level, cmsg_type, cmsg_data in ancdata:
    if cmsg_level == socket.SOL_SOCKET and cmsg_type == socket.SCM_RIGHTS:
      # There are three versions currently this script supports.
      # The oldest one: ash-chrome returns one FD, the mojo connection of
      # old bootstrap procedure (i.e., it will be BrowserService).
      # The middle one: ash-chrome returns two FDs, the mojo connection of
      # old bootstrap procedure, and the second for the start up data FD.
      # The newest one: ash-chrome returns three FDs, the mojo connection of
      # old bootstrap procedure, the second for the start up data FD, and
      # the third for another mojo connection of new bootstrap procedure.
      # TODO(crbug.com/1156033): Clean up the code to drop the support of
      # oldest one after M91.
      # TODO(crbug.com/1180712): Clean up the mojo procedure support of the
      # the middle one after M92.
      cmsg_len_candidates = [(i + 1) * fds.itemsize
                             for i in range(_NUM_FDS_MAX)]
      assert len(cmsg_data) in cmsg_len_candidates, (
          'CMSG_LEN is unexpected: %d' % (len(cmsg_data), ))
      fds.frombytes(cmsg_data[:])

  if version == b'\x00':
    assert len(fds) in (1, 2, 3), 'Expecting exactly 1, 2, or 3 FDs'
    legacy_mojo_fd = os.fdopen(fds[0])
    startup_fd = None if len(fds) < 2 else os.fdopen(fds[1])
    mojo_fd = None if len(fds) < 3 else os.fdopen(fds[2])
  elif version == b'\x01':
    assert len(fds) == 2, 'Expecting exactly 2 FDs'
    legacy_mojo_fd = None
    startup_fd = os.fdopen(fds[0])
    mojo_fd = os.fdopen(fds[1])
  else:
    raise AssertionError('Unknown version: \\x%s' % version.encode('hex'))
  return legacy_mojo_fd, startup_fd, mojo_fd


def _MaybeClosing(fileobj):
  """Returns closing context manager, if given fileobj is not None.

  If the given fileobj is none, return nullcontext.
  """
  return (contextlib.closing if fileobj else NullContext)(fileobj)


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
  flags, args = arg_parser.parse_known_args()

  assert 'XDG_RUNTIME_DIR' in os.environ
  assert os.environ.get('EGL_PLATFORM') == 'surfaceless'

  with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.connect(flags.socket_path.as_posix())
    legacy_mojo_connection, startup_connection, mojo_connection = (
        _ReceiveFDs(sock))

  with _MaybeClosing(legacy_mojo_connection), \
       _MaybeClosing(startup_connection), \
       _MaybeClosing(mojo_connection):
    cmd = args[:]
    pass_fds = []
    if legacy_mojo_connection:
      cmd.append('--mojo-platform-channel-handle=%d' %
                 legacy_mojo_connection.fileno())
      pass_fds.append(legacy_mojo_connection.fileno())
    else:
      # TODO(crbug.com/1188020): This is for backward compatibility.
      # We should remove this after M93 lacros is spread enough.
      cmd.append('--mojo-platform-channel-handle=-1')
    if startup_connection:
      cmd.append('--cros-startup-data-fd=%d' % startup_connection.fileno())
      pass_fds.append(startup_connection.fileno())
    if mojo_connection:
      cmd.append('--crosapi-mojo-platform-channel-handle=%d' %
                 mojo_connection.fileno())
      pass_fds.append(mojo_connection.fileno())
    proc = subprocess.Popen(cmd, pass_fds=pass_fds)

  return proc.wait()


if __name__ == '__main__':
  sys.exit(Main())

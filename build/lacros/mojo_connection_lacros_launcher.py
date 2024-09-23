#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors
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
      --no-startup-window --enable-features=LacrosOnly \\
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
import getpass
import grp
import os
import pathlib
import pwd
import resource
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
      # TODO(crbug.com/40735724): Clean up the code to drop the support of
      # oldest one after M91.
      # TODO(crbug.com/40170079): Clean up the mojo procedure support of the
      # the middle one after M92.
      cmsg_len_candidates = [(i + 1) * fds.itemsize
                             for i in range(_NUM_FDS_MAX)]
      assert len(cmsg_data) in cmsg_len_candidates, (
          'CMSG_LEN is unexpected: %d' % (len(cmsg_data), ))
      fds.frombytes(cmsg_data[:])

  if version == b'\x01':
    assert len(fds) == 2, 'Expecting exactly 2 FDs'
    startup_fd = os.fdopen(fds[0])
    mojo_fd = os.fdopen(fds[1])
  elif version:
    raise AssertionError('Unknown version: \\x%s' % version.hex())
  else:
    raise AssertionError('Failed to receive startup message from ash-chrome. '
                         'Make sure you\'re logged in to Chrome OS.')
  return startup_fd, mojo_fd


def _MaybeClosing(fileobj):
  """Returns closing context manager, if given fileobj is not None.

    If the given fileobj is none, return nullcontext.
    """
  return (contextlib.closing if fileobj else NullContext)(fileobj)


def _ApplyCgroups():
  """Applies cgroups used in ChromeOS to lacros chrome as well."""
  # Cgroup directories taken from ChromeOS session_manager job configuration.
  UI_FREEZER_CGROUP_DIR = '/sys/fs/cgroup/freezer/ui'
  UI_CPU_CGROUP_DIR = '/sys/fs/cgroup/cpu/ui'
  pid = os.getpid()
  with open(os.path.join(UI_CPU_CGROUP_DIR, 'tasks'), 'a') as f:
    f.write(str(pid) + '\n')
  with open(os.path.join(UI_FREEZER_CGROUP_DIR, 'cgroup.procs'), 'a') as f:
    f.write(str(pid) + '\n')


def _PreExec(uid, gid, groups):
  """Set environment up for running the chrome binary."""
  # Nice and realtime priority values taken ChromeOSs session_manager job
  # configuration.
  resource.setrlimit(resource.RLIMIT_NICE, (40, 40))
  resource.setrlimit(resource.RLIMIT_RTPRIO, (10, 10))
  os.setgroups(groups)
  os.setgid(gid)
  os.setuid(uid)


def Main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__
  arg_parser.add_argument(
      '-r',
      '--root-env-setup',
      action='store_true',
      help='Set typical cgroups and environment for chrome. '
      'If this is set, this script must be run as root.')
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

  if flags.root_env_setup:
    # Check if we are actually root and error otherwise.
    assert getpass.getuser() == 'root', \
        'Root required environment flag specified, but user is not root.'
    # Apply necessary cgroups to our own process, so they will be inherited by
    # lacros chrome.
    _ApplyCgroups()
  else:
    print('WARNING: Running chrome without appropriate environment. '
          'This may affect performance test results. '
          'Set -r and run as root to avoid this.')

  with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.connect(flags.socket_path.as_posix())
    startup_connection, mojo_connection = (_ReceiveFDs(sock))

  with _MaybeClosing(startup_connection), _MaybeClosing(mojo_connection):
    cmd = args[:]
    pass_fds = []
    if startup_connection:
      cmd.append('--cros-startup-data-fd=%d' % startup_connection.fileno())
      pass_fds.append(startup_connection.fileno())
    if mojo_connection:
      cmd.append('--crosapi-mojo-platform-channel-handle=%d' %
                 mojo_connection.fileno())
      pass_fds.append(mojo_connection.fileno())

    env = os.environ.copy()
    if flags.root_env_setup:
      username = 'chronos'
      p = pwd.getpwnam(username)
      uid = p.pw_uid
      gid = p.pw_gid
      groups = [g.gr_gid for g in grp.getgrall() if username in g.gr_mem]
      env['HOME'] = p.pw_dir
      env['LOGNAME'] = username
      env['USER'] = username

      def fn():
        return _PreExec(uid, gid, groups)
    else:

      def fn():
        return None

    proc = subprocess.Popen(cmd, pass_fds=pass_fds, preexec_fn=fn)

  return proc.wait()


if __name__ == '__main__':
  sys.exit(Main())

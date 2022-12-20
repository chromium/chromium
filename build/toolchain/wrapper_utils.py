# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions for gcc_toolchain.gni wrappers."""

import gzip
import os
import re
import subprocess
import shlex
import shutil
import sys
import threading

_BAT_PREFIX = 'cmd /c call '


def _GzipThenDelete(src_path, dest_path):
  # Results for Android map file with GCC on a z620:
  # Uncompressed: 207MB
  # gzip -9: 16.4MB, takes 8.7 seconds.
  # gzip -1: 21.8MB, takes 2.0 seconds.
  # Piping directly from the linker via -print-map (or via -Map with a fifo)
  # adds a whopping 30-45 seconds!
  with open(src_path, 'rb') as f_in, gzip.GzipFile(dest_path, 'wb', 1) as f_out:
    shutil.copyfileobj(f_in, f_out)
  os.unlink(src_path)


def CommandToRun(command):
  """Generates commands compatible with Windows.

  When running on a Windows host and using a toolchain whose tools are
  actually wrapper scripts (i.e. .bat files on Windows) rather than binary
  executables, the |command| to run has to be prefixed with this magic.
  The GN toolchain definitions take care of that for when GN/Ninja is
  running the tool directly.  When that command is passed in to this
  script, it appears as a unitary string but needs to be split up so that
  just 'cmd' is the actual command given to Python's subprocess module.

  Args:
    command: List containing the UNIX style |command|.

  Returns:
    A list containing the Windows version of the |command|.
  """
  if command[0].startswith(_BAT_PREFIX):
    command = command[0].split(None, 3) + command[1:]
  return command


def RunLinkWithOptionalMapFile(command, env=None, map_file=None):
  """Runs the given command, adding in -Wl,-Map when |map_file| is given.

  Also takes care of gzipping when |map_file| ends with .gz.

  Args:
    command: List of arguments comprising the command.
    env: Environment variables.
    map_file: Path to output map_file.

  Returns:
    The exit code of running |command|.
  """
  tmp_map_path = None
  if map_file and map_file.endswith('.gz'):
    tmp_map_path = map_file + '.tmp'
    command.append('-Wl,-Map,' + tmp_map_path)
  elif map_file:
    command.append('-Wl,-Map,' + map_file)

  # We want to link rlibs as --whole-archive if they are part of a unit test
  # target. This is determined by switch `-LinkWrapper,add-whole-archive`.
  #
  # TODO(danakj): If the linking command line gets too large we could move
  # {{rlibs}} into the rsp file, but then this script needs to modify the rsp
  # file instead of the command line.
  def extract_libname(s):
    m = re.match(r'-LinkWrapper,add-whole-archive=(.+)', s)
    return m.group(1) + ".rlib"

  # The set of libraries we want to apply `--whole-archive`` to.
  whole_archive_libs = [
      extract_libname(x) for x in command
      if x.startswith("-LinkWrapper,add-whole-archive=")
  ]

  # Remove the arguments meant for consumption by this LinkWrapper script.
  command = [x for x in command if not x.startswith("-LinkWrapper,")]

  def has_any_suffix(string, suffixes):
    for suffix in suffixes:
      if string.endswith(suffix):
        return True
    return False

  def wrap_libs_with(command, libnames, before, after):
    out = []
    for arg in command:
      # The arg is a full path to a library, we look if the the library name (a
      # suffix of the full arg) is one of `libnames`.
      if has_any_suffix(arg, libnames):
        out.extend([before, arg, after])
      else:
        out.append(arg)
    return out

  # Apply --whole-archive to the libraries that desire it.
  command = wrap_libs_with(command, whole_archive_libs, "-Wl,--whole-archive",
                           "-Wl,--no-whole-archive")

  result = subprocess.call(command, env=env)

  if tmp_map_path and result == 0:
    threading.Thread(
        target=lambda: _GzipThenDelete(tmp_map_path, map_file)).start()
  elif tmp_map_path and os.path.exists(tmp_map_path):
    os.unlink(tmp_map_path)

  return result


def CaptureCommandStderr(command, env=None):
  """Returns the stderr of a command.

  Args:
    command: A list containing the command and arguments.
    env: Environment variables for the new process.
  """
  child = subprocess.Popen(command, stderr=subprocess.PIPE, env=env)
  _, stderr = child.communicate()
  return child.returncode, stderr

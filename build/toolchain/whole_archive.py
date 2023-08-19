# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def wrap_with_whole_archive(command, is_apple=False):
  """Modify and return `command` such that -LinkWrapper,add-whole-archive=X
  becomes a linking inclusion X (-lX) but wrapped in whole-archive
  modifiers."""

  # We want to link rlibs as --whole-archive if they are part of a unit test
  # target. This is determined by switch `-LinkWrapper,add-whole-archive`.
  #
  # TODO(danakj): If the linking command line gets too large we could move
  # {{rlibs}} into the rsp file, but then this script needs to modify the rsp
  # file instead of the command line.
  def extract_libname(s):
    m = re.match(r'-LinkWrapper,add-whole-archive=(.+)', s)
    return m.group(1)

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
        out.extend([before, arg])
        if after:
          out.append(after)
      else:
        out.append(arg)
    return out

  if is_apple:
    # Apply -force_load to the libraries that desire it.
    return wrap_libs_with(command, whole_archive_libs, "-Wl,-force_load", None)
  else:
    # Apply --whole-archive to the libraries that desire it.
    return wrap_libs_with(command, whole_archive_libs, "-Wl,--whole-archive",
                          "-Wl,--no-whole-archive")

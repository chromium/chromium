#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

from util import build_utils
from util import diff_utils

IGNORE_FLAG_PREFIXES = [
    # For cflags.
    '-DANDROID_NDK_VERSION_ROLL',
    '-DCR_LIBCXX_REVISION',
    '-I',
    '-g',
    '-fcrash-diagnostics-dir=',
    '-fprofile',
    '--no-system-header-prefix',
    '--system-header-prefix',
    '-isystem',
    '-iquote',
    '-fmodule-map',
    '-frandom-seed',
    '-c ',
    '-o ',
    '-fmodule-name=',
    '--sysroot=',
    '-fcolor-diagnostics',
    '-MF ',
    '-MD',

    # For ldflags.
    '-Wl,--thinlto-cache-dir',
    '-Wl,--thinlto-cache-policy',
    '-Wl,--thinlto-jobs',
    '-Wl,--start-lib',
    '-Wl,--end-lib',
    '-Wl,-whole-archive',
    '-Wl,-no-whole-archive',
    '-l',
    '-L',
    '-Wl,-soname',
    '-Wl,-version-script',
    '-Wl,--version-script',
    '-fdiagnostics-color',
    '-Wl,--color-diagnostics',
    '-B',
    '-Wl,--dynamic-linker',
    '-DCR_CLANG_REVISION=',
]

FLAGS_WITH_PARAMS = (
    '-Xclang',
    '-mllvm',
    '-Xclang -fdebug-compilation-dir',
    '-Xclang -add-plugin',
)


def KeepFlag(flag):
  return not any(flag.startswith(prefix) for prefix in IGNORE_FLAG_PREFIXES)


def MergeFlags(flags):
  flags = _MergeFlagsHelper(flags)
  # For double params eg: -Xclang -fdebug-compilation-dir
  flags = _MergeFlagsHelper(flags)
  return flags


def _MergeFlagsHelper(flags):
  merged_flags = []
  while flags:
    current_flag = flags.pop(0)
    if flags:
      next_flag = flags[0]
    else:
      next_flag = None
    merge_flags = False

    # Special case some flags that always come with params.
    if current_flag in FLAGS_WITH_PARAMS:
      merge_flags = True
    # Assume flags without '-' are a param.
    if next_flag and not next_flag.startswith('-'):
      merge_flags = True
    # Special case -plugin-arg prefix because it has the plugin name.
    if current_flag.startswith('-Xclang -plugin-arg'):
      merge_flags = True
    if merge_flags:
      merged_flag = '{} {}'.format(current_flag, next_flag)
      merged_flags.append(merged_flag)
      flags.pop(0)
    else:
      merged_flags.append(current_flag)
  return merged_flags


def ParseFlags(flag_file_path):
  flags = []
  with open(flag_file_path) as f:
    for flag in f.read().splitlines():
      if KeepFlag(flag):
        flags.append(flag)
  return flags


def main():
  """Compare the flags with the checked in list."""
  parser = argparse.ArgumentParser()
  diff_utils.AddCommandLineFlags(parser)
  parser.add_argument('--current-flags',
                      help='Path to flags to check against expectations.')
  options = parser.parse_args()

  flags = ParseFlags(options.current_flags)
  flags = MergeFlags(flags)

  msg = """
This expectation file is meant to inform the build team about changes to
flags used when building native libraries in chrome (most importantly any
that relate to security). This is to ensure the flags are replicated when
building native libraries outside of the repo. Please update the .expected
files and a WATCHLIST entry will alert the build team to your change."""
  diff_utils.CheckExpectations('\n'.join(sorted(flags)),
                               options,
                               custom_msg=msg)


if __name__ == '__main__':
  main()

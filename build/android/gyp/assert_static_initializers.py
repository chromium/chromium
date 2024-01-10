#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks the number of static initializers in an APK's library."""


import argparse
import os
import re
import subprocess
import sys

from util import build_utils

_DUMP_STATIC_INITIALIZERS_PATH = os.path.join(build_utils.DIR_SOURCE_ROOT,
                                              'tools', 'linux',
                                              'dump-static-initializers.py')


def _RunReadelf(so_path, options, tool_prefix=''):
  return subprocess.check_output(
      [tool_prefix + 'readobj', '--elf-output-style=GNU'] + options +
      [so_path]).decode('utf8')


def _DumpStaticInitializers(so_path):
  subprocess.check_call([_DUMP_STATIC_INITIALIZERS_PATH, so_path])


def _ReadInitArray(so_path, tool_prefix):
  stdout = _RunReadelf(so_path, ['-SW'], tool_prefix)
  # Matches: .init_array INIT_ARRAY 000000000516add0 5169dd0 000010 00 WA 0 0 8
  match = re.search(r'\.init_array.*$', stdout, re.MULTILINE)
  if not match:
    raise Exception('Did not find section: .init_array in {}:\n{}'.format(
        so_path, stdout))
  size_str = re.split(r'\W+', match.group(0))[5]
  return int(size_str, 16)


def _CountStaticInitializers(so_path, tool_prefix):
  # Find the number of files with at least one static initializer.
  # First determine if we're 32 or 64 bit
  stdout = _RunReadelf(so_path, ['-h'], tool_prefix)
  elf_class_line = re.search('Class:.*$', stdout, re.MULTILINE).group(0)
  elf_class = re.split(r'\W+', elf_class_line)[1]
  if elf_class == 'ELF32':
    word_size = 4
  else:
    word_size = 8

  # Then find the number of files with global static initializers.
  # NOTE: this is very implementation-specific and makes assumptions
  # about how compiler and linker implement global static initializers.
  init_array_size = _ReadInitArray(so_path, tool_prefix)
  assert init_array_size % word_size == 0
  return init_array_size // word_size


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--touch', help='File to touch upon success')
  parser.add_argument('--tool-prefix', required=True,
                      help='Prefix for nm and friends')
  parser.add_argument('--expected-count', required=True, type=int,
                      help='Fail if number of static initializers is not '
                           'equal to this value.')
  parser.add_argument('--unstripped-so-path',
                      help='Path to the unstripped version of the .so '
                      'file if needed for better dumps.')
  parser.add_argument('so_path', help='Path to .so file.')
  args = parser.parse_args()

  si_count = _CountStaticInitializers(args.so_path, args.tool_prefix)
  if si_count != args.expected_count:
    print('Expected {} static initializers, but found {}.'.format(
        args.expected_count, si_count))
    if args.expected_count > si_count:
      print('You have removed one or more static initializers. Thanks!')
      print('To fix the build, update the expectation in:')
      print('    //chrome/android/static_initializers.gni')
      print()

    print('Dumping static initializers via dump-static-initializers.py:')
    sys.stdout.flush()
    dump_so_path = args.so_path
    if args.unstripped_so_path:
      dump_so_path = args.unstripped_so_path
    _DumpStaticInitializers(dump_so_path)
    print()
    print('For more information:')
    print('    https://chromium.googlesource.com/chromium/src/+/main/docs/'
          'static_initializers.md')
    sys.exit(1)

  if args.touch:
    open(args.touch, 'w')


if __name__ == '__main__':
  main()

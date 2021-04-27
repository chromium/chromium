#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks the number of static initializers in an APK's library."""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys
import tempfile
import zipfile

from util import build_utils

_DUMP_STATIC_INITIALIZERS_PATH = os.path.join(build_utils.DIR_SOURCE_ROOT,
                                              'tools', 'linux',
                                              'dump-static-initializers.py')


def _RunReadelf(so_path, options, tool_prefix=''):
  return subprocess.check_output([tool_prefix + 'readelf'] + options +
                                 [so_path]).decode('utf8')


def _ParseLibBuildId(so_path, tool_prefix):
  """Returns the Build ID of the given native library."""
  stdout = _RunReadelf(so_path, ['-n'], tool_prefix)
  match = re.search(r'Build ID: (\w+)', stdout)
  return match.group(1) if match else None


def _VerifyLibBuildIdsMatch(tool_prefix, *so_files):
  if len(set(_ParseLibBuildId(f, tool_prefix) for f in so_files)) > 1:
    raise Exception('Found differing build ids in output directory and apk. '
                    'Your output directory is likely stale.')


def _GetStaticInitializers(so_path, tool_prefix):
  output = subprocess.check_output(
      [_DUMP_STATIC_INITIALIZERS_PATH, '-d', so_path, '-t', tool_prefix])
  summary = re.search(r'Found \d+ static initializers in (\d+) files.', output)
  return output.splitlines()[:-1], int(summary.group(1))


def _PrintDumpSIsCount(apk_so_name, unzipped_so, out_dir, tool_prefix):
  lib_name = os.path.basename(apk_so_name).replace('crazy.', '')
  so_with_symbols_path = os.path.join(out_dir, 'lib.unstripped', lib_name)
  if not os.path.exists(so_with_symbols_path):
    raise Exception('Unstripped .so not found. Looked here: %s',
                    so_with_symbols_path)
  _VerifyLibBuildIdsMatch(tool_prefix, unzipped_so, so_with_symbols_path)
  sis, _ = _GetStaticInitializers(so_with_symbols_path, tool_prefix)
  for si in sis:
    print(si)


# Mostly copied from //infra/scripts/legacy/scripts/slave/chromium/sizes.py.
def _ReadInitArray(so_path, tool_prefix, expect_no_initializers):
  stdout = _RunReadelf(so_path, ['-SW'], tool_prefix)
  # Matches: .init_array INIT_ARRAY 000000000516add0 5169dd0 000010 00 WA 0 0 8
  match = re.search(r'\.init_array.*$', stdout, re.MULTILINE)
  if expect_no_initializers:
    if match:
      raise Exception(
          'Expected no initializers for %s, yet some were found' % so_path)
    else:
      return 0
  elif not match:
    raise Exception('Did not find section: .init_array in {}:\n{}'.format(
        so_path, stdout))
  size_str = re.split(r'\W+', match.group(0))[5]
  return int(size_str, 16)


def _CountStaticInitializers(so_path, tool_prefix, expect_no_initializers):
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
  init_array_size = _ReadInitArray(so_path, tool_prefix, expect_no_initializers)
  return init_array_size / word_size


def _AnalyzeStaticInitializers(apk_or_aab, tool_prefix, dump_sis, out_dir,
                               ignored_libs, no_initializers_libs):
  # Static initializer counting mostly copies logic in
  # infra/scripts/legacy/scripts/slave/chromium/sizes.py.
  with zipfile.ZipFile(apk_or_aab) as z:
    so_files = [
        f for f in z.infolist() if f.filename.endswith('.so')
        and f.file_size > 0 and os.path.basename(f.filename) not in ignored_libs
    ]
    # Skip checking static initializers for secondary abi libs. They will be
    # checked by 32-bit bots. This avoids the complexity of finding 32 bit .so
    # files in the output directory in 64 bit builds.
    has_64 = any('64' in f.filename for f in so_files)
    files_to_check = [f for f in so_files if not has_64 or '64' in f.filename]

    # Do not check partitioned libs. They have no ".init_array" section since
    # all SIs are considered "roots" by the linker, and so end up in the base
    # module.
    files_to_check = [
        f for f in files_to_check if not f.filename.endswith('_partition.so')
    ]

    si_count = 0
    for f in files_to_check:
      lib_basename = os.path.basename(f.filename)
      expect_no_initializers = lib_basename in no_initializers_libs
      with tempfile.NamedTemporaryFile(prefix=lib_basename) as temp:
        temp.write(z.read(f))
        temp.flush()
        si_count += _CountStaticInitializers(temp.name, tool_prefix,
                                             expect_no_initializers)
        if dump_sis:
          # Print count and list of SIs reported by dump-static-initializers.py.
          # Doesn't work well on all archs (particularly arm), which is why
          # the readelf method is used for tracking SI counts.
          _PrintDumpSIsCount(f.filename, temp.name, out_dir, tool_prefix)
  return si_count


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--touch', help='File to touch upon success')
  parser.add_argument('--tool-prefix', required=True,
                      help='Prefix for nm and friends')
  parser.add_argument('--expected-count', required=True, type=int,
                      help='Fail if number of static initializers is not '
                           'equal to this value.')
  parser.add_argument('apk_or_aab', help='Path to .apk or .aab file.')
  args = parser.parse_args()

  # TODO(crbug.com/838414): add support for files included via loadable_modules.
  ignored_libs = {
      'libarcore_sdk_c.so', 'libcrashpad_handler_trampoline.so',
      'libsketchology_native.so'
  }
  # The chromium linker doesn't have static initializers, which makes the
  # regular check throw. It should not have any.
  no_initializers_libs = ['libchromium_android_linker.so']

  si_count = _AnalyzeStaticInitializers(args.apk_or_aab, args.tool_prefix,
                                        False, '.', ignored_libs,
                                        no_initializers_libs)
  if si_count != args.expected_count:
    print('Expected {} static initializers, but found {}.'.format(
        args.expected_count, si_count))
    if args.expected_count > si_count:
      print('You have removed one or more static initializers. Thanks!')
      print('To fix the build, update the expectation in:')
      print('    //chrome/android/static_initializers.gni')
    else:
      print('Dumping static initializers via dump-static-initializers.py:')
      sys.stdout.flush()
      _AnalyzeStaticInitializers(args.apk_or_aab, args.tool_prefix, True, '.',
                                 ignored_libs, no_initializers_libs)
      print()
      print('If the above list is not useful, consider listing them with:')
      print('    //tools/binary_size/diagnose_bloat.py')
      print()
      print('For more information:')
      print('    https://chromium.googlesource.com/chromium/src/+/master/docs/'
            'static_initializers.md')
    sys.exit(1)

  if args.touch:
    open(args.touch, 'w')


if __name__ == '__main__':
  main()

#!/usr/bin/env vpython3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Implements Chrome-Fuchsia package binary size differ.'''

import argparse
import json
import os
import sys
import traceback

from binary_sizes import ReadPackageSizesJson
from binary_sizes import PACKAGES_SIZES_FILE

# Eng is not responsible for changes that cause "reasonable growth" if the
# uncompressed binary size does not grow.
# First-warning will fail the test if the uncompressed and compressed size
# grow, while always-fail will fail the test regardless of uncompressed growth
# (solely based on compressed growth).
_FIRST_WARNING_DELTA_BYTES = 12 * 1024  # 12 KiB
_ALWAYS_FAIL_DELTA_BYTES = 100 * 1024  # 100 KiB
_TRYBOT_DOC = 'https://chromium.googlesource.com/chromium/src/+/main/docs/speed/binary_size/fuchsia_binary_size_trybot.md'

SIZE_FAILURE = 1
ROLLER_SIZE_WARNING = 2
SUCCESS = 0


def ComputePackageDiffs(before_sizes_file, after_sizes_file, author=None):
  '''Computes difference between after and before diff, for each package.'''
  before_sizes = ReadPackageSizesJson(before_sizes_file)
  after_sizes = ReadPackageSizesJson(after_sizes_file)

  assert before_sizes.keys() == after_sizes.keys(), (
      'Package files cannot'
      ' be compared with different packages: '
      '{} vs {}'.format(before_sizes.keys(), after_sizes.keys()))

  growth = {'compressed': {}, 'uncompressed': {}}
  status_code = SUCCESS
  summary = ''
  for package_name in before_sizes:
    growth['compressed'][package_name] = (after_sizes[package_name].compressed -
                                          before_sizes[package_name].compressed)
    growth['uncompressed'][package_name] = (
        after_sizes[package_name].uncompressed -
        before_sizes[package_name].uncompressed)
    # Developers are only responsible if uncompressed increases.
    if ((growth['compressed'][package_name] >= _FIRST_WARNING_DELTA_BYTES
         and growth['uncompressed'][package_name] > 0)
        # However, if compressed growth is unusually large, fail always.
        or growth['compressed'][package_name] >= _ALWAYS_FAIL_DELTA_BYTES):
      if not summary:
        summary = ('Size check failed! The following package(s) are affected:'
                   '<br>')
      status_code = SIZE_FAILURE
      summary += (('- {} (compressed) grew by {} bytes (uncompressed growth:'
                   ' {} bytes).<br>').format(
                       package_name, growth['compressed'][package_name],
                       growth['uncompressed'][package_name]))
      summary += ('Note that this bot compares growth against trunk, and is '
                  'not aware of CL chaining.<br>')

  # Allow rollers to pass even with size increases. See crbug.com/1355914.
  if author and '-autoroll' in author and status_code == SIZE_FAILURE:
    summary = summary.replace('Size check failed! ', '')
    summary = (
        'The following growth by an autoroller will be ignored:<br><br>' +
        summary)
    status_code = ROLLER_SIZE_WARNING
  growth['status_code'] = status_code
  summary += ('<br>See the following document for more information about'
              ' this trybot:<br>{}'.format(_TRYBOT_DOC))
  growth['summary'] = summary

  # TODO(crbug.com/40801868): Investigate using these fields.
  growth['archive_filenames'] = []
  growth['links'] = []
  return growth


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--before-dir',
      type=os.path.realpath,
      required=True,
      help='Location of the build without the patch',
  )
  parser.add_argument(
      '--after-dir',
      type=os.path.realpath,
      required=True,
      help='Location of the build with the patch',
  )
  parser.add_argument('--author', help='Author of change')
  parser.add_argument(
      '--results-path',
      type=os.path.realpath,
      required=True,
      help='Output path for the trybot result .json file',
  )
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable verbose output')
  args = parser.parse_args()

  if args.verbose:
    print('Fuchsia binary sizes')
    print('Working directory', os.getcwd())
    print('Args:')
    for var in vars(args):
      print('  {}: {}'.format(var, getattr(args, var) or ''))

  if not os.path.isdir(args.before_dir) or not os.path.isdir(args.after_dir):
    raise Exception(
        'Could not find build output directory "{}" or "{}".'.format(
            args.before_dir, args.after_dir))

  test_name = 'sizes'
  before_sizes_file = os.path.join(args.before_dir, test_name,
                                   PACKAGES_SIZES_FILE)
  after_sizes_file = os.path.join(args.after_dir, test_name,
                                  PACKAGES_SIZES_FILE)
  if not os.path.isfile(before_sizes_file):
    raise Exception(
        'Could not find before sizes file: "{}"'.format(before_sizes_file))

  if not os.path.isfile(after_sizes_file):
    raise Exception(
        'Could not find after sizes file: "{}"'.format(after_sizes_file))

  test_completed = False
  try:
    growth = ComputePackageDiffs(before_sizes_file,
                                 after_sizes_file,
                                 author=args.author)
    test_completed = True
    with open(args.results_path, 'wt') as results_file:
      json.dump(growth, results_file)
  except:
    _, value, trace = sys.exc_info()
    traceback.print_tb(trace)
    print(str(value))
  finally:
    return 0 if test_completed else 1


if __name__ == '__main__':
  sys.exit(main())

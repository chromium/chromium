#!/usr/bin/env vpython3
#
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Implements Chrome-Fuchsia package binary size differ.'''

import argparse
import collections
import copy
import json
import logging
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
import uuid

from common import GetHostToolPathFromPlatform, GetHostArchFromPlatform
from common import SDK_ROOT, DIR_SOURCE_ROOT
from binary_sizes import GetPackageSizes, ReadPackageBlobsJson
from binary_sizes import PACKAGES_SIZE_FILE

_MAX_DELTA_BYTES = 12 * 1024  # 12 KiB


def GetPackageBlobsFromFile(blob_file_path):
  return GetPackageSizes(ReadPackageBlobsJson(blob_file_path))


def ComputePackageDiffs(before_blobs_file, after_blobs_file):
  '''Computes difference between after and before diff, for each package.'''
  before_blobs = GetPackageBlobsFromFile(before_blobs_file)
  after_blobs = GetPackageBlobsFromFile(after_blobs_file)

  assert before_blobs.keys() == after_blobs.keys(), (
      'Package files cannot'
      ' be compared with different packages: '
      '%s vs %s' % (before_blobs.keys(), after_blobs.keys()))

  growth = {'compressed': {}, 'uncompressed': {}}
  status_code = 0
  summary = ''
  for package_name in before_blobs:
    growth['compressed'][package_name] = (after_blobs[package_name].compressed -
                                          before_blobs[package_name].compressed)
    growth['uncompressed'][package_name] = (
        after_blobs[package_name].uncompressed -
        before_blobs[package_name].uncompressed)
    if growth['compressed'][package_name] >= _MAX_DELTA_BYTES:
      if status_code == 1:
        summary = 'Failed! '
      status_code = 1
      summary += ('%s grew by %d bytes' %
                  (package_name, growth['compressed'][package_name]))

  growth['status_code'] = status_code
  growth['summary'] = summary

  # TODO(crbug.com/1266085): Investigate using these fields.
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
    raise Exception('Could not find build output directory "%s" or "%s".' %
                    (args.before_dir, args.after_dir))

  test_name = 'sizes'
  before_blobs_file = os.path.join(args.before_dir, test_name,
                                   PACKAGES_SIZE_FILE)
  after_blobs_file = os.path.join(args.after_dir, test_name, PACKAGES_SIZE_FILE)
  if not os.path.isfile(before_blobs_file):
    raise Exception('Could not find before blobs file: "%s"' %
                    (before_blobs_file))

  if not os.path.isfile(after_blobs_file):
    raise Exception('Could not find after blobs file: "%s"' %
                    (after_blobs_file))

  test_completed = False
  try:
    growth = ComputePackageDiffs(before_blobs_file, after_blobs_file)
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

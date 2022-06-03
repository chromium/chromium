#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""Prints the lowest locally available SDK version greater than or equal to a
given minimum sdk version to standard output.

If --print_sdk_path is passed, then the script will also print the SDK path.
If --print_bin_path is passed, then the script will also print the path to the
toolchain bin dir.

Usage:
  python find_sdk.py     \
      [--print_sdk_path] \
      [--print_bin_path] \
      10.6  # Ignores SDKs < 10.6

Sample Output:
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.sdk
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/
10.14
"""

from __future__ import print_function

import os
import re
import subprocess
import sys

from optparse import OptionParser


class SdkError(Exception):
  def __init__(self, value):
    self.value = value
  def __str__(self):
    return repr(self.value)


def parse_version(version_str):
  """'10.6' => [10, 6]"""
  return [int(s) for s in re.findall(r'(\d+)', version_str)]


def main():
  parser = OptionParser()
  parser.add_option("--print_sdk_path",
                    action="store_true", dest="print_sdk_path", default=False,
                    help="Additionally print the path the SDK (appears first).")
  parser.add_option("--print_bin_path",
                    action="store_true", dest="print_bin_path", default=False,
                    help="Additionally print the path the toolchain bin dir.")
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Please specify a minimum SDK version')
  min_sdk_version = args[0]


  job = subprocess.Popen(['xcode-select', '-print-path'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
  out, err = job.communicate()
  if job.returncode != 0:
    print(out, file=sys.stderr)
    print(err, file=sys.stderr)
    raise Exception('Error %d running xcode-select' % job.returncode)
  dev_dir = out.decode('UTF-8').rstrip()
  sdk_dir = os.path.join(
      dev_dir, 'Platforms/MacOSX.platform/Developer/SDKs')

  if not os.path.isdir(sdk_dir):
    raise SdkError('Install Xcode, launch it, accept the license ' +
      'agreement, and run `sudo xcode-select -s /path/to/Xcode.app` ' +
      'to continue.')
  sdks = [re.findall('^MacOSX(\d+\.\d+)\.sdk$', s) for s in os.listdir(sdk_dir)]
  sdks = [s[0] for s in sdks if s]  # [['10.5'], ['10.6']] => ['10.5', '10.6']
  sdks = [s for s in sdks  # ['10.5', '10.6'] => ['10.6']
          if parse_version(s) >= parse_version(min_sdk_version)]
  if not sdks:
    raise Exception('No %s+ SDK found' % min_sdk_version)
  best_sdk = sorted(sdks, key=parse_version)[0]

  if options.print_sdk_path:
    sdk_name = 'MacOSX' + best_sdk + '.sdk'
    print(os.path.join(sdk_dir, sdk_name))

  if options.print_bin_path:
    bin_path = 'Toolchains/XcodeDefault.xctoolchain/usr/bin/'
    print(os.path.join(dev_dir, bin_path))

  return best_sdk


if __name__ == '__main__':
  if sys.platform != 'darwin':
    raise Exception("This script only runs on Mac")
  print(main())
  sys.exit(0)

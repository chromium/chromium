#!/usr/bin/env python3

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Prints "1" if Chrome targets should be built with hermetic Xcode.
Prints "2" if Chrome targets should be built with hermetic Xcode, but the OS
version does not meet the minimum requirements of the hermetic version of Xcode.
Prints "3" if FORCE_MAC_TOOLCHAIN is set for an iOS target_os
Otherwise prints "0".

Usage:
  python should_use_hermetic_xcode.py <target_os>
"""


import argparse
import os
import sys

_THIS_DIR_PATH = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
_BUILD_PATH = os.path.join(_THIS_DIR_PATH, os.pardir)
sys.path.insert(0, _BUILD_PATH)

import mac_toolchain


def _IsCorpMachine():
  if sys.platform == 'darwin':
    return os.path.isdir('/Library/GoogleCorpSupport/')
  if sys.platform.startswith('linux'):
    import subprocess
    try:
      return subprocess.check_output(['lsb_release',
                                      '-sc']).rstrip() == b'rodete'
    except:
      return False
  return False


def main():
  parser = argparse.ArgumentParser(description='Download hermetic Xcode.')
  parser.add_argument('platform')
  args = parser.parse_args()

  force_toolchain = os.environ.get('FORCE_MAC_TOOLCHAIN')
  if force_toolchain and args.platform == 'ios':
    return "3"
  allow_corp = args.platform == 'mac' and _IsCorpMachine()
  if force_toolchain or allow_corp:
    if not mac_toolchain.PlatformMeetsHermeticXcodeRequirements():
      return "2"
    return "1"
  else:
    return "0"


if __name__ == '__main__':
  print(main())
  sys.exit(0)

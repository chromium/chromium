#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import doctest
import itertools
import os
import plistlib
import re
import subprocess
import sys


# This script prints information about the build system, the operating
# system and the iOS or Mac SDK (depending on the platform "iphonesimulator",
# "iphoneos" or "macosx" generally).


def SplitVersion(version):
  """Splits the Xcode version to 3 values.

  >>> list(SplitVersion('8.2.1.1'))
  ['8', '2', '1']
  >>> list(SplitVersion('9.3'))
  ['9', '3', '0']
  >>> list(SplitVersion('10.0'))
  ['10', '0', '0']
  """
  version = version.split('.')
  return itertools.islice(itertools.chain(version, itertools.repeat('0')), 0, 3)


def FormatVersion(version):
  """Converts Xcode version to a format required for DTXcode in Info.plist

  >>> FormatVersion('8.2.1')
  '0821'
  >>> FormatVersion('9.3')
  '0930'
  >>> FormatVersion('10.0')
  '1000'
  """
  major, minor, patch = SplitVersion(version)
  return ('%2s%s%s' % (major, minor, patch)).replace(' ', '0')


def FillXcodeVersion(settings, developer_dir):
  """Fills the Xcode version and build number into |settings|."""
  if developer_dir:
    xcode_version_plist_path = os.path.join(developer_dir,
                                            'Contents/version.plist')
    with open(xcode_version_plist_path, 'rb') as f:
      version_plist = plistlib.load(f)
    settings['xcode_version'] = FormatVersion(
        version_plist['CFBundleShortVersionString'])
    settings['xcode_version_int'] = int(settings['xcode_version'], 10)
    settings['xcode_build'] = version_plist['ProductBuildVersion']
    return

  lines = subprocess.check_output(['xcodebuild',
                                   '-version']).decode('UTF-8').splitlines()
  settings['xcode_version'] = FormatVersion(lines[0].split()[-1])
  settings['xcode_version_int'] = int(settings['xcode_version'], 10)
  settings['xcode_build'] = lines[-1].split()[-1]


def FillMachineOSBuild(settings):
  """Fills OS build number into |settings|."""
  machine_os_build = subprocess.check_output(['sw_vers', '-buildVersion'
                                              ]).decode('UTF-8').strip()
  settings['machine_os_build'] = machine_os_build


def FillSDKPathAndVersion(settings, platform, xcode_version):
  """Fills the SDK path and version for |platform| into |settings|."""
  settings['sdk_path'] = subprocess.check_output(
      ['xcrun', '-sdk', platform, '--show-sdk-path']).decode('UTF-8').strip()
  settings['sdk_version'] = subprocess.check_output(
      ['xcrun', '-sdk', platform,
       '--show-sdk-version']).decode('UTF-8').strip()
  settings['sdk_platform_path'] = subprocess.check_output(
      ['xcrun', '-sdk', platform,
       '--show-sdk-platform-path']).decode('UTF-8').strip()
  settings['sdk_build'] = subprocess.check_output(
      ['xcrun', '-sdk', platform,
       '--show-sdk-build-version']).decode('UTF-8').strip()
  settings['toolchains_path'] = os.path.join(
      subprocess.check_output(['xcode-select',
                               '-print-path']).decode('UTF-8').strip(),
      'Toolchains/XcodeDefault.xctoolchain')


def CreateXcodeSymlinkAt(src, dst, root_build_dir):
  """Create symlink to Xcode directory at target location."""

  if not os.path.isdir(dst):
    os.makedirs(dst)

  dst = os.path.join(dst, os.path.basename(src))
  updated_value = os.path.join(root_build_dir, dst)

  # Update the symlink only if it is different from the current destination.
  if os.path.islink(dst):
    current_src = os.readlink(dst)
    if current_src == src:
      return updated_value
    os.unlink(dst)
    sys.stderr.write('existing symlink %s points %s; want %s. Removed.' %
                     (dst, current_src, src))
  os.symlink(src, dst)
  return updated_value


def main():
  doctest.testmod()

  parser = argparse.ArgumentParser()
  parser.add_argument('--developer_dir')
  parser.add_argument('--get_sdk_info',
                      action='store_true',
                      default=False,
                      help='Returns SDK info in addition to xcode info.')
  parser.add_argument('--get_machine_info',
                      action='store_true',
                      default=False,
                      help='Returns machine info in addition to xcode info.')
  parser.add_argument('--create_symlink_at',
                      help='Create symlink of SDK at given location and '
                      'returns the symlinked paths as SDK info instead '
                      'of the original location.')
  parser.add_argument('--root_build_dir',
                      default='.',
                      help='Value of gn $root_build_dir')
  parser.add_argument('platform',
                      choices=['iphoneos', 'iphonesimulator', 'macosx'])
  args = parser.parse_args()
  if args.developer_dir:
    os.environ['DEVELOPER_DIR'] = args.developer_dir

  settings = {}
  if args.get_machine_info:
    FillMachineOSBuild(settings)
  FillXcodeVersion(settings, args.developer_dir)
  if args.get_sdk_info:
    FillSDKPathAndVersion(settings, args.platform, settings['xcode_version'])

  for key in sorted(settings):
    value = settings[key]
    if args.create_symlink_at and '_path' in key:
      value = CreateXcodeSymlinkAt(value, args.create_symlink_at,
                                   args.root_build_dir)
    if isinstance(value, str):
      value = '"%s"' % value
    print('%s=%s' % (key, value))


if __name__ == '__main__':
  sys.exit(main())

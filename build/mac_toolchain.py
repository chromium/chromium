#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
If should_use_hermetic_xcode.py emits "1", and the current toolchain is out of
date:
  * Downloads the hermetic mac toolchain
    * Requires CIPD authentication. Run `cipd auth-login`, use Google account.
  * Accepts the license.
    * If xcode-select and xcodebuild are not passwordless in sudoers, requires
      user interaction.
  * Downloads standalone binaries from [a possibly different version of Xcode].

The toolchain version can be overridden by setting MAC_TOOLCHAIN_REVISION with
the full revision, e.g. 9A235.
"""

from __future__ import print_function

import argparse
import os
import pkg_resources
import platform
import plistlib
import shutil
import subprocess
import sys

# To build these packages, see comments in build/xcode_binaries.yaml
MAC_BINARIES_LABEL = 'infra_internal/ios/xcode/xcode_binaries/mac-amd64'
MAC_BINARIES_TAG = {
    # This contains binaries from Xcode 11.2.1, along with the 10.15 SDKs (aka
    # 11B53).
    'default': 'wXywrnOhzFxwLYlwO62UzRxVCjnu6DoSI2D2jrCd00gC',
    # This contains binaries from the Xcode 12.2 release candidate, along with
    # the macOS 11 SDK (aka 12B5044c).
    'xcode_12_beta': 'UwKVijd1FvMzmCAjyoYq_sw6xXJZpw_mGBH420gwlrwC',
}

# The toolchain will not be downloaded if the minimum OS version is not met.
# 17 is the major version number for macOS 10.13.
# 9E145 (Xcode 9.3) only runs on 10.13.2 and newer.
MAC_MINIMUM_OS_VERSION = {
    'default': [17],  # macOS 10.13+
    'xcode_12_beta': [19, 4],  # macOS 10.15.4+
}

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
TOOLCHAIN_ROOT = os.path.join(BASE_DIR, 'mac_files')
TOOLCHAIN_BUILD_DIR = os.path.join(TOOLCHAIN_ROOT, 'Xcode.app')

# Always integrity-check the entire SDK. Mac SDK packages are complex and often
# hit edge cases in cipd (eg https://crbug.com/1033987,
# https://crbug.com/915278), and generally when this happens it requires manual
# intervention to fix.
# Note the trailing \n!
PARANOID_MODE = '$ParanoidMode CheckIntegrity\n'


def PlatformMeetsHermeticXcodeRequirements(version):
  if sys.platform != 'darwin':
    return True
  needed = MAC_MINIMUM_OS_VERSION[version]
  major_version = [int(v) for v in platform.release().split('.')[:len(needed)]]
  return major_version >= needed


def _UseHermeticToolchain():
  current_dir = os.path.dirname(os.path.realpath(__file__))
  script_path = os.path.join(current_dir, 'mac/should_use_hermetic_xcode.py')
  proc = subprocess.Popen([script_path, 'mac'], stdout=subprocess.PIPE)
  return '1' in proc.stdout.readline()


def RequestCipdAuthentication():
  """Requests that the user authenticate to access Xcode CIPD packages."""

  print('Access to Xcode CIPD package requires authentication.')
  print('-----------------------------------------------------------------')
  print()
  print('You appear to be a Googler.')
  print()
  print('I\'m sorry for the hassle, but you may need to do a one-time manual')
  print('authentication. Please run:')
  print()
  print('    cipd auth-login')
  print()
  print('and follow the instructions.')
  print()
  print('NOTE: Use your google.com credentials, not chromium.org.')
  print()
  print('-----------------------------------------------------------------')
  print()
  sys.stdout.flush()


def PrintError(message):
  # Flush buffers to ensure correct output ordering.
  sys.stdout.flush()
  sys.stderr.write(message + '\n')
  sys.stderr.flush()


def InstallXcodeBinaries(version, binaries_root=None):
  """Installs the Xcode binaries needed to build Chrome and accepts the license.

  This is the replacement for InstallXcode that installs a trimmed down version
  of Xcode that is OS-version agnostic.
  """
  # First make sure the directory exists. It will serve as the cipd root. This
  # also ensures that there will be no conflicts of cipd root.
  if binaries_root is None:
    binaries_root = os.path.join(TOOLCHAIN_ROOT, 'xcode_binaries')
  if not os.path.exists(binaries_root):
    os.makedirs(binaries_root)

  # 'cipd ensure' is idempotent.
  args = [
      'cipd', 'ensure', '-root', binaries_root, '-ensure-file', '-'
  ]

  p = subprocess.Popen(
      args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  stdout, stderr = p.communicate(input=PARANOID_MODE + MAC_BINARIES_LABEL +
                                 ' ' + MAC_BINARIES_TAG[version])
  if p.returncode != 0:
    print(stdout)
    print(stderr)
    RequestCipdAuthentication()
    return 1

  if sys.platform != 'darwin':
    return 0

  # Accept the license for this version of Xcode if it's newer than the
  # currently accepted version.
  cipd_xcode_version_plist_path = os.path.join(
      binaries_root, 'Contents/version.plist')
  cipd_xcode_version_plist = plistlib.readPlist(cipd_xcode_version_plist_path)
  cipd_xcode_version = cipd_xcode_version_plist['CFBundleShortVersionString']

  cipd_license_path = os.path.join(
      binaries_root, 'Contents/Resources/LicenseInfo.plist')
  cipd_license_plist = plistlib.readPlist(cipd_license_path)
  cipd_license_version = cipd_license_plist['licenseID']

  should_overwrite_license = True
  current_license_path = '/Library/Preferences/com.apple.dt.Xcode.plist'
  if os.path.exists(current_license_path):
    current_license_plist = plistlib.readPlist(current_license_path)
    xcode_version = current_license_plist.get(
        'IDEXcodeVersionForAgreedToGMLicense')
    if (xcode_version is not None and pkg_resources.parse_version(xcode_version)
        >= pkg_resources.parse_version(cipd_xcode_version)):
      should_overwrite_license = False

  if not should_overwrite_license:
    return 0

  # Use puppet's sudoers script to accept the license if its available.
  license_accept_script = '/usr/local/bin/xcode_accept_license.py'
  if os.path.exists(license_accept_script):
    args = ['sudo', license_accept_script, '--xcode-version',
            cipd_xcode_version, '--license-version', cipd_license_version]
    subprocess.check_call(args)
    return 0

  # Otherwise manually accept the license. This will prompt for sudo.
  print('Accepting new Xcode license. Requires sudo.')
  sys.stdout.flush()
  args = ['sudo', 'defaults', 'write', current_license_path,
          'IDEXcodeVersionForAgreedToGMLicense', cipd_xcode_version]
  subprocess.check_call(args)
  args = ['sudo', 'defaults', 'write', current_license_path,
          'IDELastGMLicenseAgreedTo', cipd_license_version]
  subprocess.check_call(args)
  args = ['sudo', 'plutil', '-convert', 'xml1', current_license_path]
  subprocess.check_call(args)

  return 0


def main():
  if not _UseHermeticToolchain():
    print('Skipping Mac toolchain installation for mac')
    return 0

  parser = argparse.ArgumentParser(description='Download hermetic Xcode.')
  parser.add_argument('--xcode-version',
                      choices=('default', 'xcode_12_beta'),
                      default='default')
  args = parser.parse_args()

  if not PlatformMeetsHermeticXcodeRequirements(args.xcode_version):
    print('OS version does not support toolchain.')
    return 0

  return InstallXcodeBinaries(args.xcode_version)


if __name__ == '__main__':
  sys.exit(main())

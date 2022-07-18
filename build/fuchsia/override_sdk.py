#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check out the Fuchsia SDK from a given GCS path. Should be used in a
'hooks_os' entry so that it only runs when .gclient's custom_vars includes
'fuchsia'."""

import argparse
import logging
import os
import platform
import sys

from common import GetHostOsFromPlatform, SDK_ROOT
from update_images import DownloadAndUnpackFromCloudStorage, \
                          MakeCleanDirectory


def _GetHostArch():
  host_arch = platform.machine()
  # platform.machine() returns AMD64 on 64-bit Windows.
  if host_arch in ['x86_64', 'AMD64']:
    return 'amd64'
  elif host_arch == 'aarch64':
    return 'arm64'
  raise Exception('Unsupported host architecture: %s' % host_arch)


def _GetTarballPath(gcs_tarball_prefix: str) -> str:
  """Get the full path to the sdk tarball on GCS"""
  platform = GetHostOsFromPlatform()
  arch = _GetHostArch()
  return f'{gcs_tarball_prefix}/{platform}-{arch}/gn.tar.gz'


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Exit if there's no SDK support for this platform.
  try:
    GetHostOsFromPlatform()
  except:
    logging.warning('Fuchsia SDK is not supported on this platform.')
    return 0

  sdk_override = os.path.join(os.path.dirname(__file__), 'sdk_override.txt')

  # Exit if there is no override file.
  if not os.path.isfile(sdk_override):
    return 0

  with open(sdk_override, 'r') as f:
    gcs_tarball_prefix = f.read()

  # Always re-download the SDK.
  logging.info('Downloading GN SDK...')
  MakeCleanDirectory(SDK_ROOT)
  DownloadAndUnpackFromCloudStorage(_GetTarballPath(gcs_tarball_prefix),
                                    SDK_ROOT)
  return 0


if __name__ == '__main__':
  sys.exit(main())

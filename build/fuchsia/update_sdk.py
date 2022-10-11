#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check out the Fuchsia SDK from a given GCS path. Should be used in a
'hooks_os' entry so that it only runs when .gclient's custom_vars includes
'fuchsia'."""

import argparse
import logging
import os
import platform
import subprocess
import sys
from typing import Optional

from common import GetHostOsFromPlatform
from common import MakeCleanDirectory
from common import SDK_ROOT

from gcs_download import DownloadAndUnpackFromCloudStorage


def _GetHostArch():
  host_arch = platform.machine()
  # platform.machine() returns AMD64 on 64-bit Windows.
  if host_arch in ['x86_64', 'AMD64']:
    return 'amd64'
  elif host_arch == 'aarch64':
    return 'arm64'
  raise Exception('Unsupported host architecture: %s' % host_arch)


def GetSDKOverrideGCSPath(path: Optional[str] = None) -> Optional[str]:
  """Fetches the sdk override path from a file.

  Args:
    path: the full file path to read the data from.
      defaults to sdk_override.txt in the directory of this file.

  Returns:
    The contents of the file, stripped of white space.
      Example: gs://fuchsia-artifacts/development/some-id/sdk
  """
  if not path:
    path = os.path.join(os.path.dirname(__file__), 'sdk_override.txt')

  if not os.path.isfile(path):
    return None

  with open(path, 'r') as f:
    return f.read().strip()


def _GetTarballPath(gcs_tarball_prefix: str) -> str:
  """Get the full path to the sdk tarball on GCS"""
  platform = GetHostOsFromPlatform()
  arch = _GetHostArch()
  return f'{gcs_tarball_prefix}/{platform}-{arch}/gn.tar.gz'


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--cipd-prefix', help='CIPD base directory for the SDK.')
  parser.add_argument('--version', help='Specifies the SDK version.')
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Exit if there's no SDK support for this platform.
  try:
    host_plat = GetHostOsFromPlatform()
  except:
    logging.warning('Fuchsia SDK is not supported on this platform.')
    return 0

  gcs_tarball_prefix = GetSDKOverrideGCSPath()

  # Download from CIPD if there is no override file.
  if not gcs_tarball_prefix:
    if not args.cipd_prefix:
      parser.exit(1, '--cipd-prefix must be specified.')
    if not args.version:
      parser.exit(2, '--version must be specified.')
    logging.info('Downloading GN SDK from CIPD...')
    ensure_file = '%s%s-%s %s' % (args.cipd_prefix, host_plat, _GetHostArch(),
                                  args.version)
    subprocess.run(('cipd', 'ensure', '-ensure-file', '-', '-root', SDK_ROOT,
                    '-log-level', 'warning'),
                   check=True,
                   text=True,
                   input=ensure_file)
    return 0

  # Always re-download the SDK.
  logging.info('Downloading GN SDK from GCS...')
  MakeCleanDirectory(SDK_ROOT)
  DownloadAndUnpackFromCloudStorage(_GetTarballPath(gcs_tarball_prefix),
                                    SDK_ROOT)
  return 0


if __name__ == '__main__':
  sys.exit(main())

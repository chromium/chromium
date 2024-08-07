#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Check out the Fuchsia SDK from a given GCS path. Should be used in a
'hooks_os' entry so that it only runs when .gclient's custom_vars includes
'fuchsia'."""

import argparse
import json
import logging
import os
import platform
import subprocess
import sys
from typing import Optional

from gcs_download import DownloadAndUnpackFromCloudStorage

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

from common import SDK_ROOT, get_host_os, make_clean_directory

_VERSION_FILE = os.path.join(SDK_ROOT, 'meta', 'manifest.json')


def _GetHostArch():
  host_arch = platform.machine()
  # platform.machine() returns AMD64 on 64-bit Windows.
  if host_arch in ['x86_64', 'AMD64']:
    return 'amd64'
  elif host_arch == 'aarch64':
    return 'arm64'
  raise Exception('Unsupported host architecture: %s' % host_arch)


def GetSDKOverrideGCSPath() -> Optional[str]:
  """Fetches the sdk override path from a file or an environment variable.

  Returns:
    The override sdk location, stripped of white space.
      Example: gs://fuchsia-artifacts/development/some-id/sdk
  """
  if os.getenv('FUCHSIA_SDK_OVERRIDE'):
    return os.environ['FUCHSIA_SDK_OVERRIDE'].strip()

  path = os.path.join(os.path.dirname(__file__), 'sdk_override.txt')

  if os.path.isfile(path):
    with open(path, 'r') as f:
      return f.read().strip()

  return None


def _GetCurrentVersionFromManifest() -> Optional[str]:
  if not os.path.exists(_VERSION_FILE):
    return None
  with open(_VERSION_FILE) as f:
    return json.load(f)['id']


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--cipd-prefix', help='CIPD base directory for the SDK.')
  parser.add_argument('--version', help='Specifies the SDK version.')
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  parser.add_argument(
      '--file',
      help='Specifies the sdk tar.gz file name without .tar.gz suffix',
      default='core')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Exit if there's no SDK support for this platform.
  try:
    host_plat = get_host_os()
  except:
    logging.warning('Fuchsia SDK is not supported on this platform.')
    return 0

  # TODO(crbug.com/326004432): Remove this once DEPS have been fixed not to
  # include the "version:" prefix.
  if args.version.startswith('version:'):
    args.version = args.version[len('version:'):]

  gcs_tarball_prefix = GetSDKOverrideGCSPath()
  if not gcs_tarball_prefix:
    # sdk_override contains the full path but not only the version id. But since
    # the scenario is limited to dry-run, it's not worth complexity to extract
    # the version id.
    if args.version == _GetCurrentVersionFromManifest():
      return 0

  make_clean_directory(SDK_ROOT)

  # Download from CIPD if there is no override file.
  if not gcs_tarball_prefix:
    if not args.cipd_prefix:
      parser.exit(1, '--cipd-prefix must be specified.')
    if not args.version:
      parser.exit(2, '--version must be specified.')
    logging.info('Downloading SDK from CIPD...')
    ensure_file = '%s%s-%s version:%s' % (args.cipd_prefix, host_plat,
                                          _GetHostArch(), args.version)
    subprocess.run(('cipd', 'ensure', '-ensure-file', '-', '-root', SDK_ROOT,
                    '-log-level', 'warning'),
                   check=True,
                   text=True,
                   input=ensure_file)

    # Verify that the downloaded version matches the expected one.
    downloaded_version = _GetCurrentVersionFromManifest()
    if downloaded_version != args.version:
      logging.error(
          'SDK version after download does not match expected (downloaded:%s '
          'vs expected:%s)', downloaded_version, args.version)
      return 3
  else:
    logging.info('Downloading SDK from GCS...')
    DownloadAndUnpackFromCloudStorage(
        f'{gcs_tarball_prefix}/{get_host_os()}-{_GetHostArch()}/'
        f'{args.file}.tar.gz', SDK_ROOT)

  # Build rules (e.g. fidl_library()) depend on updates to the top-level
  # manifest to spot when to rebuild for an SDK update. Ensure that ninja
  # sees that the SDK manifest has changed, regardless of the mtime set by
  # the download & unpack steps above, by setting mtime to now.
  # See crbug.com/1457463
  os.utime(os.path.join(SDK_ROOT, 'meta', 'manifest.json'), None)

  root_dir = os.path.dirname(os.path.realpath(__file__))
  build_def_cmd = [
      os.path.join(root_dir, 'gen_build_defs.py'),
  ]
  subprocess.run(build_def_cmd, check=True)

  return 0


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the Fuchsia SDK to the given revision. Should be used in a 'hooks_os'
entry so that it only runs when .gclient's target_os includes 'fuchsia'."""

import argparse
import itertools
import logging
import os
import re
import shutil
import subprocess
import sys
import tarfile

from common import GetHostOsFromPlatform, GetHostArchFromPlatform, \
                   DIR_SOURCE_ROOT, IMAGES_ROOT
from update_sdk import DownloadAndUnpackFromCloudStorage, \
                       GetOverrideCloudStorageBucket, GetSdkHash, \
                       MakeCleanDirectory, SDK_SIGNATURE_FILE


def GetSdkSignature(sdk_hash, boot_images):
  return 'gn:{sdk_hash}:{boot_images}:'.format(sdk_hash=sdk_hash,
                                               boot_images=boot_images)


def GetAllImages(boot_image_names):
  if not boot_image_names:
    return

  all_device_types = ['generic', 'qemu']
  all_archs = ['x64', 'arm64']

  images_to_download = set()

  for boot_image in boot_image_names.split(','):
    components = boot_image.split('.')
    if len(components) != 2:
      continue

    device_type, arch = components
    device_images = all_device_types if device_type == '*' else [device_type]
    arch_images = all_archs if arch == '*' else [arch]
    images_to_download.update(itertools.product(device_images, arch_images))
  return images_to_download


def DownloadSdkBootImages(bucket, sdk_hash, boot_image_names, image_root_dir):
  images_to_download = GetAllImages(boot_image_names)
  for image_to_download in images_to_download:
    device_type = image_to_download[0]
    arch = image_to_download[1]
    image_output_dir = os.path.join(image_root_dir, arch, device_type)
    if os.path.exists(image_output_dir):
      continue

    logging.info('Downloading Fuchsia boot images for %s.%s...' %
                 (device_type, arch))
    if bucket == 'fuchsia-sdk':
      images_tarball_url = 'gs://{bucket}/development/{sdk_hash}/images/'\
          '{device_type}.{arch}.tgz'.format(
              bucket=bucket, sdk_hash=sdk_hash,
              device_type=device_type, arch=arch)
    else:
      images_tarball_url = 'gs://{bucket}/development/{sdk_hash}/images/'\
          '{device_type}-{arch}.tgz'.format(
              bucket=bucket, sdk_hash=sdk_hash,
              device_type=device_type, arch=arch)
    DownloadAndUnpackFromCloudStorage(images_tarball_url, image_output_dir)


def GetNewSignature(sdk_hash, boot_images):
  return GetSdkSignature(sdk_hash, boot_images)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  parser.add_argument(
      '--boot-images',
      type=str,
      required=True,
      help='List of boot images to download, represented as a comma separated '
      'list. Wildcards are allowed. ')
  parser.add_argument(
      '--default-bucket',
      type=str,
      default='fuchsia',
      help='The Google Cloud Storage bucket in which the Fuchsia images are '
      'stored. Entry in sdk-bucket.txt will override this flag.')
  parser.add_argument(
      '--image-root-dir',
      default=IMAGES_ROOT,
      help='Specify the root directory of the downloaded images. Optional')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # If no boot images need to be downloaded, exit.
  if not args.boot_images:
    return 0

  # Check whether there's SDK support for this platform.
  GetHostOsFromPlatform()

  # Use the bucket in sdk-bucket.txt if an entry exists.
  # Otherwise use the default bucket.
  bucket = GetOverrideCloudStorageBucket() or args.default_bucket

  sdk_hash = GetSdkHash(bucket)
  if not sdk_hash:
    return 1

  signature_filename = os.path.join(args.image_root_dir, SDK_SIGNATURE_FILE)
  current_signature = (open(signature_filename, 'r').read().strip()
                       if os.path.exists(signature_filename) else '')
  new_signature = GetNewSignature(sdk_hash, args.boot_images)
  if current_signature != new_signature:
    logging.info('Downloading Fuchsia images %s...' % sdk_hash)
    MakeCleanDirectory(args.image_root_dir)

    try:
      DownloadSdkBootImages(bucket, sdk_hash, args.boot_images,
                            args.image_root_dir)
      with open(signature_filename, 'w') as f:
        f.write(new_signature)

    except subprocess.CalledProcessError as e:
      logging.error(("command '%s' failed with status %d.%s"), " ".join(e.cmd),
                    e.returncode, " Details: " + e.output if e.output else "")

  return 0


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Fuchsia SDK to the given revision. Should be used in a 'hooks_os'
entry so that it only runs when .gclient's target_os includes 'fuchsia'."""

from __future__ import print_function

import argparse
import itertools
import logging
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

from common import GetHostOsFromPlatform, GetHostArchFromPlatform, \
                   DIR_SOURCE_ROOT, SDK_ROOT, IMAGES_ROOT

sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build'))

import find_depot_tools

SDK_SUBDIRS = ["arch", "pkg", "qemu", "sysroot", "target",
               "toolchain_libs", "tools"]

EXTRA_SDK_HASH_PREFIX = ''

def GetSdkGeneration(hash):
  if not hash:
    return None

  cmd = [os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'ls',
         '-L', GetSdkBucketForPlatform() + hash]
  logging.debug("Running '%s'", " ".join(cmd))
  sdk_details = subprocess.check_output(cmd)
  m = re.search('Generation:\s*(\d*)', sdk_details)
  if not m:
    return None
  return int(m.group(1))


def GetSdkHashForPlatform():
  filename = '{platform}.sdk.sha1'.format(platform =  GetHostOsFromPlatform())

  # Get the hash of the SDK in chromium.
  sdk_hash = None
  hash_file = os.path.join(os.path.dirname(__file__), filename)
  with open(hash_file, 'r') as f:
    sdk_hash = f.read().strip()

  # Get the hash of the SDK with the extra prefix.
  extra_sdk_hash = None
  if EXTRA_SDK_HASH_PREFIX:
    extra_hash_file = os.path.join(os.path.dirname(__file__),
                                   EXTRA_SDK_HASH_PREFIX + filename)
    with open(extra_hash_file, 'r') as f:
      extra_sdk_hash = f.read().strip()

  # If both files are empty, return an error.
  if not sdk_hash and not extra_sdk_hash:
    print(
        'No SHA1 found in {} or {}'.format(hash_file, extra_hash_file),
        file=sys.stderr)
    return 1

  # Return the newer SDK based on the generation number.
  sdk_generation = GetSdkGeneration(sdk_hash)
  extra_sdk_generation = GetSdkGeneration(extra_sdk_hash)
  if extra_sdk_generation > sdk_generation:
    return extra_sdk_hash
  return sdk_hash


def GetSdkBucketForPlatform():
  return 'gs://fuchsia/sdk/core/{platform}-amd64/'.format(
      platform = GetHostOsFromPlatform())


def EnsureDirExists(path):
  if not os.path.exists(path):
    os.makedirs(path)


# Removes legacy SDK if it's detected.
def CleanupLegacySDK():
  legacy_sdk_root = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk')
  hash_file = os.path.join(legacy_sdk_root, '.hash')
  if os.path.exists(hash_file):
    print('Removing legacy SDK.')
    for d in SDK_SUBDIRS:
      to_remove = os.path.join(legacy_sdk_root, d)
      if os.path.isdir(to_remove):
        shutil.rmtree(to_remove)
    os.remove(hash_file)


# Updates the modification timestamps of |path| and its contents to the
# current time.
def UpdateTimestampsRecursive():
  for root, dirs, files in os.walk(SDK_ROOT):
    for f in files:
      os.utime(os.path.join(root, f), None)
    for d in dirs:
      os.utime(os.path.join(root, d), None)


# Fetches a tarball from GCS and uncompresses it to |output_dir|.
def DownloadAndUnpackFromCloudStorage(url, output_dir):
  # Pass the compressed stream directly to 'tarfile'; don't bother writing it
  # to disk first.
  cmd = [os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'),
         'cp', url, '-']
  logging.debug('Running "%s"', ' '.join(cmd))
  task = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
  try:
    tarfile.open(mode='r|gz', fileobj=task.stdout).extractall(path=output_dir)
  except tarfile.ReadError:
    task.wait()
    stderr = task.stderr.read()
    raise subprocess.CalledProcessError(task.returncode, cmd,
      "Failed to read a tarfile from gsutil.py.{}".format(
        stderr if stderr else ""))
  task.wait()
  if task.returncode:
    raise subprocess.CalledProcessError(task.returncode, cmd,
                                        task.stderr.read())


def DownloadSdkBootImages(sdk_hash, boot_image_names):
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
    device_images = all_device_types if device_type=='*' else [device_type]
    arch_images = all_archs if arch=='*' else [arch]
    images_to_download.update(itertools.product(device_images, arch_images))

  for image_to_download in images_to_download:
    device_type = image_to_download[0]
    arch = image_to_download[1]
    image_output_dir = os.path.join(IMAGES_ROOT, arch, device_type)
    if os.path.exists(image_output_dir):
      continue

    print('Downloading Fuchsia boot images for %s.%s...' % (device_type, arch))
    images_tarball_url = \
        'gs://fuchsia/development/{sdk_hash}/images/'\
        '{device_type}-{arch}.tgz'.format(
            sdk_hash=sdk_hash, device_type=device_type, arch=arch)
    DownloadAndUnpackFromCloudStorage(images_tarball_url, image_output_dir)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose', '-v',
    action='store_true',
    help='Enable debug-level logging.')
  parser.add_argument('--boot-images',
    type=str, nargs='?',
    help='List of boot images to download, represented as a comma separated '
         'list. Wildcards are allowed. '
         'If omitted, no boot images will be downloaded.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARNING)

  # Quietly exit if there's no SDK support for this platform.
  try:
    GetHostOsFromPlatform()
  except:
    return 0

  # Previously SDK was unpacked in //third_party/fuchsia-sdk instead of
  # //third_party/fuchsia-sdk/sdk . Remove the old files if they are still
  # there.
  CleanupLegacySDK()

  sdk_hash = GetSdkHashForPlatform()
  if not sdk_hash:
    return 1

  hash_filename = os.path.join(SDK_ROOT, '.hash')
  if os.path.exists(hash_filename):
    with open(hash_filename, 'r') as f:
      if f.read().strip() == sdk_hash:
        # Ensure that the boot images are downloaded for this SDK.
        # If the developer opted into downloading hardware boot images in their
        # .gclient file, then only the hardware boot images will be downloaded.
        DownloadSdkBootImages(sdk_hash, args.boot_images)

        # Nothing to do. Generate sdk/BUILD.gn anyway, in case the conversion
        # script changed.
        logging.info("Generating sdk/BUILD.gn")
        cmd = [os.path.join(SDK_ROOT, '..', 'gen_build_defs.py')]
        logging.debug("Running '%s'", " ".join(cmd))
        subprocess.check_call(cmd)
        return 0

  print('Downloading SDK %s...' % sdk_hash)

  if os.path.isdir(SDK_ROOT):
    shutil.rmtree(SDK_ROOT)

  fd, tmp = tempfile.mkstemp()
  os.close(fd)

  EnsureDirExists(SDK_ROOT)
  DownloadAndUnpackFromCloudStorage(GetSdkBucketForPlatform() + sdk_hash,
                                   SDK_ROOT)

  # Generate sdk/BUILD.gn.
  cmd = [os.path.join(SDK_ROOT, '..', 'gen_build_defs.py')]
  logging.debug("Running '%s'", " ".join(cmd))
  subprocess.check_call(cmd)

  # Clean out the boot images directory.
  if (os.path.exists(IMAGES_ROOT)):
    shutil.rmtree(IMAGES_ROOT)
  os.mkdir(IMAGES_ROOT)

  try:
    DownloadSdkBootImages(sdk_hash, args.boot_images)
  except subprocess.CalledProcessError as e:
    logging.error((
      "command '%s' failed with status %d.%s"),
      " ".join(e.cmd), e.returncode,
      " Details: " + e.output if e.output else "")
    return 1

  with open(hash_filename, 'w') as f:
    f.write(sdk_hash)

  UpdateTimestampsRecursive()

  return 0


if __name__ == '__main__':
  sys.exit(main())

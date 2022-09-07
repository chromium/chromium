#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the Fuchsia images to the given revision. Should be used in a
'hooks_os' entry so that it only runs when .gclient's target_os includes
'fuchsia'."""

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

sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build'))
import find_depot_tools

IMAGE_SIGNATURE_FILE = '.hash'


def DownloadAndUnpackFromCloudStorage(url, output_dir):
  """Fetches a tarball from GCS and uncompresses it to |output_dir|."""

  # Pass the compressed stream directly to 'tarfile'; don't bother writing it
  # to disk first.
  cmd = [
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'cp', url,
      '-'
  ]
  logging.debug('Running "%s"', ' '.join(cmd))
  task = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
  try:
    tarfile.open(mode='r|gz', fileobj=task.stdout).extractall(path=output_dir)
  except tarfile.ReadError as exc:
    task.wait()
    stderr = task.stderr.read()
    raise subprocess.CalledProcessError(
        task.returncode, cmd,
        "Failed to read a tarfile from gsutil.py.\n{}".format(
            stderr if stderr else "")) from exc
  task.wait()
  if task.returncode:
    raise subprocess.CalledProcessError(task.returncode, cmd,
                                        task.stderr.read())


# TODO(crbug.com/1138433): Investigate whether we can deprecate
# use of sdk_bucket.txt.
def GetOverrideCloudStorageBucket():
  """Read bucket entry from sdk_bucket.txt"""
  return ReadFile('sdk-bucket.txt').strip()


def MakeCleanDirectory(directory_name):
  if (os.path.exists(directory_name)):
    shutil.rmtree(directory_name)
  os.mkdir(directory_name)


def ReadFile(filename):
  """Read a file in this directory."""
  with open(os.path.join(os.path.dirname(__file__), filename), 'r') as f:
    return f.read()


def StrExpansion():
  return lambda str_value: str_value


def VarLookup(local_scope):
  return lambda var_name: local_scope['vars'][var_name]


def GetImageHashList(bucket):
  """Read filename entries from sdk-hash-files.list (one per line), substitute
  {platform} in each entry if present, and read from each filename."""
  assert (GetHostOsFromPlatform() == 'linux')
  filenames = [
      line.strip() for line in ReadFile('sdk-hash-files.list').replace(
          '{platform}', 'linux_internal').splitlines()
  ]
  image_hashes = [ReadFile(filename).strip() for filename in filenames]
  return image_hashes


def ParseDepsDict(deps_content):
  local_scope = {}
  global_scope = {
      'Str': StrExpansion(),
      'Var': VarLookup(local_scope),
      'deps_os': {},
  }
  exec(deps_content, global_scope, local_scope)
  return local_scope


def ParseDepsFile(filename):
  with open(filename, 'rb') as f:
    deps_content = f.read()
  return ParseDepsDict(deps_content)


def GetImageHash(bucket):
  """Gets the hash identifier of the newest generation of images."""
  if bucket == 'fuchsia-sdk':
    hashes = GetImageHashList(bucket)
    return max(hashes)
  deps_file = os.path.join(DIR_SOURCE_ROOT, 'DEPS')
  return ParseDepsFile(deps_file)['vars']['fuchsia_version'].split(':')[1]


def GetImageSignature(image_hash, boot_images):
  return 'gn:{image_hash}:{boot_images}:'.format(image_hash=image_hash,
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


def DownloadBootImages(bucket, image_hash, boot_image_names, image_root_dir):
  images_to_download = GetAllImages(boot_image_names)
  for image_to_download in images_to_download:
    device_type = image_to_download[0]
    arch = image_to_download[1]
    image_output_dir = os.path.join(image_root_dir, arch, device_type)
    if os.path.exists(image_output_dir):
      continue

    logging.info('Downloading Fuchsia boot images for %s.%s...' %
                 (device_type, arch))

    # Legacy images use different naming conventions. See fxbug.dev/85552.
    legacy_delimiter_device_types = ['qemu', 'generic']
    if bucket == 'fuchsia-sdk' or \
       device_type not in legacy_delimiter_device_types:
      type_arch_connector = '.'
    else:
      type_arch_connector = '-'

    images_tarball_url = 'gs://{bucket}/development/{image_hash}/images/'\
        '{device_type}{type_arch_connector}{arch}.tgz'.format(
            bucket=bucket, image_hash=image_hash, device_type=device_type,
            type_arch_connector=type_arch_connector, arch=arch)
    try:
      DownloadAndUnpackFromCloudStorage(images_tarball_url, image_output_dir)
    except subprocess.CalledProcessError as e:
      logging.exception('Failed to download image %s from URL: %s',
                        image_to_download, images_tarball_url)


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

  # Check whether there's Fuchsia support for this platform.
  GetHostOsFromPlatform()

  # Use the bucket in sdk-bucket.txt if an entry exists.
  # Otherwise use the default bucket.
  bucket = GetOverrideCloudStorageBucket() or args.default_bucket

  image_hash = GetImageHash(bucket)
  if not image_hash:
    return 1

  signature_filename = os.path.join(args.image_root_dir, IMAGE_SIGNATURE_FILE)
  current_signature = (open(signature_filename, 'r').read().strip()
                       if os.path.exists(signature_filename) else '')
  new_signature = GetImageSignature(image_hash, args.boot_images)
  if current_signature != new_signature:
    logging.info('Downloading Fuchsia images %s...' % image_hash)
    MakeCleanDirectory(args.image_root_dir)

    try:
      DownloadBootImages(bucket, image_hash, args.boot_images,
                         args.image_root_dir)
      with open(signature_filename, 'w') as f:
        f.write(new_signature)

    except subprocess.CalledProcessError as e:
      logging.error(("command '%s' failed with status %d.%s"), " ".join(e.cmd),
                    e.returncode, " Details: " + e.output if e.output else "")

  return 0


if __name__ == '__main__':
  sys.exit(main())

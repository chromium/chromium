#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the Fuchsia product bundles to the given revision. Should be used
in a 'hooks_os' entry so that it only runs when .gclient's target_os includes
'fuchsia'."""

import argparse
import json
import logging
import os
import subprocess
import sys

from contextlib import ExitStack
import common
import ffx_session
import log_manager

_PRODUCT_BUNDLES = [
    'core.x64-dfv2',
    'terminal.qemu-arm64',
    'terminal.qemu-x64',
    'workstation_eng.chromebook-x64',
    'workstation_eng.chromebook-x64-dfv2',
    'workstation_eng.qemu-x64',
]

# TODO(crbug/1361089): Remove when the old scripts have been deprecated.
_IMAGE_TO_PRODUCT_BUNDLE = {
    'core.x64-dfv2-release': 'core.x64-dfv2',
    'qemu.arm64': 'terminal.qemu-arm64',
    'qemu.x64': 'terminal.qemu-x64',
    'workstation_eng.chromebook-x64-dfv2-release':
    'workstation_eng.chromebook-x64-dfv2',
    'workstation_eng.chromebook-x64-release': 'workstation_eng.chromebook-x64',
    'workstation_eng.qemu-x64-release': 'workstation_eng.qemu-x64',
}


# TODO(crbug/1361089): Remove when the old scripts have been deprecated.
def convert_to_product_bundle(images_list):
  """Convert image names in the SDK to product bundle names."""

  product_bundle_list = []
  for image in images_list:
    if image in _IMAGE_TO_PRODUCT_BUNDLE:
      logging.warning(f'Image name {image} has been deprecated. Use '
                      f'{_IMAGE_TO_PRODUCT_BUNDLE.get(image)} instead.')
    product_bundle_list.append(_IMAGE_TO_PRODUCT_BUNDLE.get(image, image))
  return product_bundle_list


def get_hash_from_sdk():
  """Retrieve version info from the SDK."""

  version_file = os.path.join(common.SDK_ROOT, 'meta', 'manifest.json')
  if not os.path.exists(version_file):
    raise RuntimeError('Could not detect version file. Make sure the SDK has '
                       'been downloaded')
  with open(version_file, 'r') as f:
    return json.load(f)['id']


def download_product_bundle(product_bundle, ffx_runner):
  """Download product bundles using the SDK."""

  logging.info('Downloading Fuchsia product bundle %s', product_bundle)
  try:
    ffx_runner.run_ffx(('product-bundle', 'get', product_bundle))
  except subprocess.CalledProcessError as cpe:
    logging.error('Product bundle download has failed. This could be '
                  'because an earlier version of the product bundle was not '
                  'properly removed. Run |ffx product-bundle list|, remove '
                  'the available product bundles listed using '
                  '|ffx product-bundle remove|, remove the directory '
                  f'{common.IMAGES_ROOT} and rerun hooks/this script.')
    raise


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  parser.add_argument(
      'product_bundles',
      type=str,
      help='List of product bundles to download, represented as a comma '
      'separated list.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Check whether there's Fuchsia support for this platform.
  common.GetHostOsFromPlatform()

  new_product_bundles = convert_to_product_bundle(
      args.product_bundles.split(','))
  for pb in new_product_bundles:
    if pb not in _PRODUCT_BUNDLES:
      raise ValueError(f'{pb} is not part of the Fuchsia product bundle.')

  if '*' in args.product_bundles:
    raise ValueError('Wildcards are no longer supported, all product bundles '
                     'need to be explicitly listed. The full list can be '
                     'found in the DEPS file.')

  with ExitStack() as stack:
    ffx_runner = ffx_session.FfxRunner(log_manager.LogManager(None))

    # Re-set the directory to which product bundles are downloaded so that
    # these bundles are located inside the Chromium codebase.
    ffx_runner.run_ffx(
        ('config', 'set', 'pbms.storage.path', common.IMAGES_ROOT))

    # TODO(crbug/1380807): Remove when product bundles can be downloaded
    # for custom SDKs without editing metadata
    override_file = os.path.join(os.path.dirname(__file__), 'sdk_override.txt')
    if os.path.isfile(override_file):
      with open(override_file) as f:
        pb_metadata = f.read().split('\n')
        pb_metadata.append('{sdk.root}/*.json')
      stack.enter_context(
          ffx_runner.scoped_config('pbms.metadata', json.dumps((pb_metadata))))

    signature_filename = common.PRODUCT_BUNDLE_SIGNATURE_FILE
    curr_signature = {}
    if os.path.exists(signature_filename):
      with open(signature_filename, 'r') as f:
        curr_signature = json.load(f)

    new_sdk_hash = get_hash_from_sdk()
    new_signature = {'sdk_hash': new_sdk_hash}

    # If SDK versions match, remove the product bundles that are no longer
    # needed and download missing ones.
    if curr_signature.get('sdk_hash') == new_sdk_hash:
      curr_signature.get('sdk_hash')
      new_signature['path'] = curr_signature.get('path')
      new_product_bundle_hash = []

      for image in curr_signature.get('images', []):
        if image in new_product_bundles:
          new_product_bundle_hash.append(image)
        else:
          logging.info('Removing no longer needed Fuchsia image %s' % image)
          ffx_runner.run_ffx(('product-bundle', 'remove', '-f', image))

      bundles_to_download = set(new_product_bundles) - \
                            set(curr_signature.get('images', []))
      for bundle in bundles_to_download:
        download_product_bundle(bundle, ffx_runner)
      new_product_bundle_hash.extend(bundles_to_download)
      new_signature['images'] = new_product_bundle_hash

      with open(signature_filename, 'w') as f:
        f.write(json.dumps(new_signature))
      return 0

    # If SDK versions do not match, remove all existing product bundles
    # and download the ones required.
    for pb in curr_signature.get('images', []):
      ffx_runner.run_ffx(('product-bundle', 'remove', '-f', pb))

    curr_subdir = []
    if os.path.exists(common.IMAGES_ROOT):
      curr_subdir = os.listdir(common.IMAGES_ROOT)
    common.MakeCleanDirectory(common.IMAGES_ROOT)

    for pb in new_product_bundles:
      download_product_bundle(pb, ffx_runner)
    new_signature['images'] = new_product_bundles

    new_subdir = os.listdir(common.IMAGES_ROOT)
    subdir_diff = set(new_subdir) - set(curr_subdir)
    if len(subdir_diff) != 1:
      raise RuntimeError(f'Expected one new created subdirectory but got '
                         '{subdir_diff} instead.')
    new_signature['path'] = list(subdir_diff)[0]

    with open(signature_filename, 'w') as f:
      f.write(json.dumps(new_signature))
  return 0


if __name__ == '__main__':
  sys.exit(main())

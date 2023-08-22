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
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

import common
import update_sdk


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
def convert_to_products(images_list):
  """Convert image names in the SDK to product bundle names."""

  product_bundle_list = []
  for image in images_list:
    if image in _IMAGE_TO_PRODUCT_BUNDLE:
      logging.warning(f'Image name {image} has been deprecated. Use '
                      f'{_IMAGE_TO_PRODUCT_BUNDLE.get(image)} instead.')
    product_bundle_list.append(_IMAGE_TO_PRODUCT_BUNDLE.get(image, image))
  return product_bundle_list


def remove_repositories(repo_names_to_remove):
  """Removes given repos from repo list.
  Repo MUST be present in list to succeed.

  Args:
    repo_names_to_remove: List of repo names (as strings) to remove.
  """
  for repo_name in repo_names_to_remove:
    common.run_ffx_command(cmd=('repository', 'remove', repo_name), check=True)


def get_repositories():
  """Lists repositories that are available on disk.

  Also prunes repositories that are listed, but do not have an actual packages
  directory.

  Returns:
    List of dictionaries containing info about the repositories. They have the
    following structure:
    {
      'name': <repo name>,
      'spec': {
        'type': <type, usually pm>,
        'path': <path to packages directory>
      },
    }
  """

  repos = json.loads(
      common.run_ffx_command(cmd=('--machine', 'json', 'repository', 'list'),
                             check=True,
                             capture_output=True).stdout.strip())
  to_prune = set()
  sdk_root_abspath = os.path.abspath(os.path.dirname(common.SDK_ROOT))
  for repo in repos:
    # Confirm the path actually exists. If not, prune list.
    # Also assert the product-bundle repository is for the current repo
    # (IE within the same directory).
    if not os.path.exists(repo['spec']['path']):
      to_prune.add(repo['name'])

    if not repo['spec']['path'].startswith(sdk_root_abspath):
      to_prune.add(repo['name'])

  repos = [repo for repo in repos if repo['name'] not in to_prune]

  remove_repositories(to_prune)
  return repos


def get_current_signature(image_dir):
  """Determines the current version of the image, if it exists.

  Returns:
    The current version, or None if the image is non-existent.
  """

  version_file = os.path.join(image_dir, 'product_bundle.json')
  if os.path.exists(version_file):
    with open(version_file) as f:
      return json.load(f)['product_version']
  return None


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help='Enable debug-level logging.')
  parser.add_argument(
      'products',
      type=str,
      help='List of product bundles to download, represented as a comma '
      'separated list.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Check whether there's Fuchsia support for this platform.
  common.get_host_os()

  new_products = convert_to_products(args.products.split(','))
  logging.info('Searching for the following products: %s', str(new_products))

  logging.debug('Getting new SDK hash')
  new_sdk_hash = common.get_hash_from_sdk()

  for product in new_products:
    prod, board = product.split('.', 1)
    image_dir = os.path.join(common.IMAGES_ROOT, prod, board)

    curr_signature = get_current_signature(image_dir)

    if curr_signature != new_sdk_hash:
      common.make_clean_directory(image_dir)
      logging.debug('Checking for override file')
      override_file = os.path.join(os.path.dirname(__file__),
                                   'sdk_override.txt')
      if os.path.isfile(override_file):
        base_url = update_sdk.GetSDKOverrideGCSPath().replace('/sdk', '')
      else:
        base_url = f'gs://fuchsia/development/{new_sdk_hash}'
      download_url = common.run_ffx_command(cmd=('product', 'lookup', product,
                                                 new_sdk_hash, '--base-url',
                                                 base_url),
                                            check=True,
                                            capture_output=True).stdout.strip()
      logging.info(f'Downloading {product} from {base_url}.')
      common.run_ffx_command(cmd=('product', 'download', download_url,
                                  image_dir),
                             check=True)

  return 0


if __name__ == '__main__':
  sys.exit(main())

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
from compatible_utils import running_unattended


# TODO(crbug.com/40863468): Remove when the old scripts have been deprecated.
_IMAGE_TO_PRODUCT_BUNDLE = {
    'qemu.arm64': 'terminal.qemu-arm64',
    'qemu.x64': 'terminal.x64',
}


# TODO(crbug.com/40863468): Remove when the old scripts have been deprecated.
def convert_to_products(images_list):
  """Convert image names in the SDK to product bundle names."""

  product_bundle_list = []
  for image in images_list:
    if image in _IMAGE_TO_PRODUCT_BUNDLE:
      logging.warning(f'Image name {image} has been deprecated. Use '
                      f'{_IMAGE_TO_PRODUCT_BUNDLE.get(image)} instead.')
      product_bundle_list.append(_IMAGE_TO_PRODUCT_BUNDLE[image])
    else:
      if image.endswith('-release'):
        image = image[:-len('-release')]
        logging.warning(f'Image name {image}-release has been deprecated. Use '
                        f'{image} instead.')
      product_bundle_list.append(image)
  return product_bundle_list


def remove_repositories(repo_names_to_remove):
  """Removes given repos from repo list.
  Repo MUST be present in list to succeed.

  Args:
    repo_names_to_remove: List of repo names (as strings) to remove.
  """
  for repo_name in repo_names_to_remove:
    common.run_ffx_command(cmd=('repository', 'remove', repo_name))


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


# VisibleForTesting
def internal_hash():
  hash_filename = os.path.join(os.path.dirname(__file__),
                               'linux_internal.sdk.sha1')
  return (open(hash_filename, 'r').read().strip()
          if os.path.exists(hash_filename) else '')


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
  parser.add_argument(
      '--internal',
      action='store_true',
      help='Whether the images are coming from internal, it impacts version '
      'file, bucket and download location.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  # Check whether there's Fuchsia support for this platform.
  common.get_host_os()

  new_products = convert_to_products(args.products.split(','))
  logging.debug('Searching for the following products: %s', str(new_products))

  logging.debug('Getting new SDK hash')
  if args.internal:
    new_hash = internal_hash()
  else:
    new_hash = common.get_hash_from_sdk()

  auth_args = [
      '--auth',
      os.path.join(os.path.dirname(__file__), 'get_auth_token.py')
  ] if running_unattended() else []
  for product in new_products:
    prod, board = product.split('.', 1)
    if prod.startswith('smart_display_') and board in [
        'astro', 'sherlock', 'nelson'
    ]:
      # This is a hacky way of keeping the files into the folders matching
      # the original image name, since the definition is unfortunately in
      # src-internal. Likely we can download two copies for a smooth
      # transition, but it would be easier to keep it as-is during the ffx
      # product v2 migration.
      # TODO(crbug.com/40938340): Migrate the image download folder away from
      # the following hack.
      prod, board = board + '-release', prod
    if args.internal:
      # sdk_override.txt does not work for internal images.
      override_url = None
      image_dir = os.path.join(common.INTERNAL_IMAGES_ROOT, prod, board)
    else:
      override_url = update_sdk.GetSDKOverrideGCSPath()
      if override_url:
        # TODO(zijiehe): Convert to removesuffix once python 3.9 is supported.
        if override_url.endswith('/sdk'):
          override_url = override_url[:-len('/sdk')]
        logging.debug(f'Using {override_url} from override file.')
      image_dir = os.path.join(common.IMAGES_ROOT, prod, board)
    curr_signature = get_current_signature(image_dir)

    if not override_url and curr_signature == new_hash:
      continue

    common.make_clean_directory(image_dir)
    base_url = override_url or 'gs://{bucket}/development/{new_hash}'.format(
        bucket='fuchsia-sdk' if args.internal else 'fuchsia', new_hash=new_hash)
    effective_auth_args = auth_args if base_url.startswith(
        'gs://fuchsia-artifacts-internal/') or base_url.startswith(
            'gs://fuchsia-sdk/') else []
    lookup_output = common.run_ffx_command(cmd=[
        '--machine', 'json', 'product', 'lookup', product, new_hash,
        '--base-url', base_url
    ] + effective_auth_args,
                                           capture_output=True).stdout.strip()
    download_url = json.loads(lookup_output)['transfer_manifest_url']
    # The download_url is purely a timestamp based gs location and is fairly
    # meaningless, so we log the base_url instead which contains the sdk version
    # if it's not coming from the sdk_override.txt file.
    logging.info(f'Downloading {product} from {base_url} and {download_url}.')
    common.run_ffx_command(
        cmd=['product', 'download', download_url, image_dir] +
        effective_auth_args)

  return 0


if __name__ == '__main__':
  sys.exit(main())

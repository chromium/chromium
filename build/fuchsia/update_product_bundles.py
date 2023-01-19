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
import re
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
    'workstation_eng.x64',
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


_PRODUCT_BUNDLE_FIX_INSTRUCTIONS = (
    'This could be because an earlier version of the product bundle was not '
    'properly removed. Run |ffx product-bundle list| and |ffx repository list|,'
    ' remove the available product bundles listed using '
    '|ffx product-bundle remove| and |ffx repository remove|, '
    f'remove the directory {common.IMAGES_ROOT} and rerun hooks/this script.')


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


def remove_repositories(repo_names_to_remove, ffx_runner):
  """Removes given repos from repo list.
  Repo MUST be present in list to succeed.

  Args:
    repo_names_to_remove: List of repo names (as strings) to remove.
    ffx_runner: ffx_session.FfxRunner instance to run the command.
  """
  for repo_name in repo_names_to_remove:
    ffx_runner.run_ffx(('repository', 'remove', repo_name), check=True)


def get_repositories(ffx_runner):
  """Lists repositories that are available on disk.

  Also prunes repositories that are listed, but do not have an actual packages
  directory.

  Args:
    ffx_runner: An FfxRunner instance.

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
      ffx_runner.run_ffx(('--machine', 'json', 'repository', 'list')).strip())
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

  remove_repositories(to_prune, ffx_runner)
  return repos


def update_repositories_list(ffx_runner):
  """Used to prune stale repositories."""
  get_repositories(ffx_runner)


def remove_product_bundle(product_bundle, ffx_runner):
  """Removes product-bundle given."""
  ffx_runner.run_ffx(('product-bundle', 'remove', '-f', product_bundle))


def get_product_bundle_urls(ffx_runner):
  """Retrieves URLs of available product-bundles.

  Args:
    ffx_runner: An FfxRunner instance.

  Returns:
    List of dictionaries of structure, indicating whether the product-bundle
    has been downloaded.
    {
      'url': <GCS path of product-bundle>,
      'downloaded': <True|False>
    }
  """
  # TODO(fxb/115328): Replaces with JSON API when available.
  bundles = ffx_runner.run_ffx(('product-bundle', 'list'), check=True)

  urls = [
      line.strip() for line in bundles.splitlines() if 'gs://fuchsia' in line
  ]
  structured_urls = []
  for url in urls:
    downloaded = False
    if '*' in url:
      downloaded = True
      url = url.split(' ')[1]
    structured_urls.append({'downloaded': downloaded, 'url': url.strip()})
  return structured_urls


def keep_product_bundles_by_sdk_version(sdk_version, ffx_runner):
  """Prunes product bundles not containing the sdk_version given."""
  urls = get_product_bundle_urls(ffx_runner)
  for url in urls:
    if url['downloaded'] and sdk_version not in url['url']:
      remove_product_bundle(url['url'], ffx_runner)


def get_product_bundles(ffx_runner):
  """Lists all downloaded product-bundles for the given SDK.

  Cross-references the repositories with downloaded packages and the stated
  downloaded product-bundles to validate whether or not a product-bundle is
  present. Prunes invalid product-bundles with each call as well.

  Args:
    ffx_runner: An FfxRunner instance.

  Returns:
    List of strings of product-bundle names downloaded and that FFX is aware
    of.
  """
  downloaded_bundles = []

  for url in get_product_bundle_urls(ffx_runner):
    if url['downloaded']:
      # The product is separated by a #
      product = url['url'].split('#')
      downloaded_bundles.append(product[1])

  # For each downloaded bundle, need to verify whether ffx repository believes
  # it exists.
  to_prune_bundles_index = []
  repos = get_repositories(ffx_runner)

  # Some repo names do not match product-bundle names due to underscores.
  # Normalize them both.
  repo_names = set([repo['name'].replace('-', '_') for repo in repos])

  def bundle_is_active(name):
    # Returns True if the product-bundle named `name` is present in a package
    # repository (assuming it is downloaded already); otherwise, removes the
    # product-bundle and returns False.
    if name.replace('-', '_') in repo_names:
      return True

    remove_product_bundle(name, ffx_runner)
    return False

  return list(filter(bundle_is_active, downloaded_bundles))


def download_product_bundle(product_bundle, ffx_runner):
  """Download product bundles using the SDK."""
  # This also updates the repository list, in case it is stale.
  update_repositories_list(ffx_runner)

  try:
    ffx_runner.run_ffx(
        ('product-bundle', 'get', product_bundle, '--force-repo'))
  except subprocess.CalledProcessError as cpe:
    logging.error('Product bundle download has failed. ' +
                  _PRODUCT_BUNDLE_FIX_INSTRUCTIONS)
    raise


def get_current_signature(ffx_runner):
  """Determines the SDK version of the product-bundles associated with the SDK.

  Parses this information from the URLs of the product-bundle.

  Args:
    ffx_runner: An FfxRunner instance.

  Returns:
    An SDK version string, or None if no product-bundle versions are downloaded.
  """
  product_bundles = get_product_bundles(ffx_runner)
  if not product_bundles:
    logging.info('No product bundles - signature will default to None')
    return None
  product_bundle_urls = get_product_bundle_urls(ffx_runner)

  # Get the numbers, hope they're the same.
  signatures = set()
  for bundle in product_bundle_urls:
    m = re.search(r'/(\d+\.\d+\.\d+.\d+|\d+)/', bundle['url'])
    assert m, 'Must have a signature in each URL'
    signatures.add(m.group(1))

  if len(signatures) > 1:
    raise RuntimeError('Found more than one product signature. ' +
                       _PRODUCT_BUNDLE_FIX_INSTRUCTIONS)

  return next(iter(signatures)) if signatures else None


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
  logging.info('Searching for the following product bundles: %s',
               str(new_product_bundles))
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

    logging.debug('Checking for override file')

    # TODO(crbug/1380807): Remove when product bundles can be downloaded
    # for custom SDKs without editing metadata
    override_file = os.path.join(os.path.dirname(__file__), 'sdk_override.txt')
    if os.path.isfile(override_file):
      with open(override_file) as f:
        pb_metadata = f.read().strip().split('\n')
        pb_metadata.append('{sdk.root}/*.json')
      stack.enter_context(
          ffx_runner.scoped_config('pbms.metadata', json.dumps((pb_metadata))))
      logging.debug('Applied overrides')

    logging.debug('Getting new SDK hash')
    new_sdk_hash = get_hash_from_sdk()
    keep_product_bundles_by_sdk_version(new_sdk_hash, ffx_runner)
    logging.debug('Checking for current signature')
    curr_signature = get_current_signature(ffx_runner)

    current_images = get_product_bundles(ffx_runner)

    # If SDK versions match, remove the product bundles that are no longer
    # needed and download missing ones.
    if curr_signature == new_sdk_hash:
      logging.debug('Current images: %s, desired images %s',
                    str(current_images), str(new_product_bundles))
      for image in current_images:
        if image not in new_product_bundles:
          logging.debug('Removing no longer needed Fuchsia image %s' % image)
          remove_product_bundle(image, ffx_runner)

      bundles_to_download = set(new_product_bundles) - \
                            set(current_images)
      for bundle in bundles_to_download:
        logging.debug('Downloading image: %s', image)
        download_product_bundle(bundle, ffx_runner)

      return 0

    # If SDK versions do not match, remove all existing product bundles
    # and download the ones required.
    for pb in current_images:
      remove_product_bundle(pb, ffx_runner)

    logging.debug('Make clean images root')
    curr_subdir = []
    if os.path.exists(common.IMAGES_ROOT):
      curr_subdir = os.listdir(common.IMAGES_ROOT)
    common.MakeCleanDirectory(common.IMAGES_ROOT)

    for pb in new_product_bundles:
      logging.debug('Downloading bundle: %s', pb)
      download_product_bundle(pb, ffx_runner)

    current_pb = get_product_bundles(ffx_runner)

    diff = set(current_pb) - set(new_product_bundles)
    assert set(current_pb) == set(new_product_bundles), (
        'Failed to download expected set of product-bundles. '
        f'Expected {new_product_bundles}, got {current_pb}')

  return 0


if __name__ == '__main__':
  sys.exit(main())

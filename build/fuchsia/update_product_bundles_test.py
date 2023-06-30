#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import os
import sys
import unittest
from unittest import mock

from parameterized import parameterized

import update_product_bundles

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

import common


class TestUpdateProductBundles(unittest.TestCase):
  def setUp(self):
    ffx_mock = mock.Mock()
    ffx_mock.returncode = 0
    self._ffx_patcher = mock.patch('common.run_ffx_command',
                                   return_value=ffx_mock)
    self._ffx_mock = self._ffx_patcher.start()
    self.addCleanup(self._ffx_mock.stop)

  def testConvertToProductBundleDefaultsUnknownImage(self):
    self.assertEqual(
        update_product_bundles.convert_to_products(['unknown-image']),
        ['unknown-image'])

  def testConvertToProductBundleWarnsDeprecated(self):
    with self.assertLogs(level='WARNING') as logs:
      deprecated_images = [
          'qemu.arm64', 'qemu.x64', 'core.x64-dfv2-release',
          'workstation_eng.chromebook-x64-release'
      ]
      self.assertEqual(
          update_product_bundles.convert_to_products(deprecated_images), [
              'terminal.qemu-arm64', 'terminal.qemu-x64', 'core.x64-dfv2',
              'workstation_eng.chromebook-x64'
          ])
      for i, deprecated_image in enumerate(deprecated_images):
        self.assertIn(f'Image name {deprecated_image} has been deprecated',
                      logs.output[i])


  @mock.patch('common.run_ffx_command')
  def testRemoveRepositoriesRunsRemoveOnGivenRepos(self, ffx_mock):
    update_product_bundles.remove_repositories(['foo', 'bar', 'fizz', 'buzz'])

    ffx_mock.assert_has_calls([
        mock.call(cmd=('repository', 'remove', 'foo'), check=True),
        mock.call(cmd=('repository', 'remove', 'bar'), check=True),
        mock.call(cmd=('repository', 'remove', 'fizz'), check=True),
        mock.call(cmd=('repository', 'remove', 'buzz'), check=True),
    ])

  @mock.patch('os.path.exists')
  @mock.patch('os.path.abspath')
  def testGetRepositoriesPrunesReposThatDoNotExist(self, mock_abspath,
                                                   mock_exists):
    with mock.patch('common.SDK_ROOT', 'some/path'):
      self._ffx_mock.return_value.stdout = json.dumps([{
          "name": "terminal.qemu-x64",
          "spec": {
              "type": "pm",
              "path": "some/path/that/exists"
          }
      }, {
          "name": "workstation-eng.chromebook-x64",
          "spec": {
              "type": "pm",
              "path": "some/path/that/does/not/exist"
          }
      }])
      mock_exists.side_effect = [True, False]
      mock_abspath.side_effect = lambda x: x

      self.assertEqual(update_product_bundles.get_repositories(), [{
          "name": "terminal.qemu-x64",
          "spec": {
              "type": "pm",
              "path": "some/path/that/exists"
          }
      }])

      self._ffx_mock.assert_has_calls([
          mock.call(cmd=('--machine', 'json', 'repository', 'list'),
                    capture_output=True,
                    check=True),
          mock.call(cmd=('repository', 'remove',
                         'workstation-eng.chromebook-x64'),
                    check=True)
      ])

  def testRemoveProductBundle(self):
    update_product_bundles.remove_product_bundle('some-bundle-foo-bar')

    self._ffx_mock.assert_called_once_with(cmd=('product-bundle', 'remove',
                                                '-f', 'some-bundle-foo-bar'))

  def _InitFFXRunWithProductBundleList(self, sdk_version='10.20221114.2.1'):
    self._ffx_mock.return_value.stdout = f"""
  gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.qemu-x64
  gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.chromebook-x64-dfv2
* gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.chromebook-x64
* gs://fuchsia/{sdk_version}/bundles.json#terminal.qemu-x64
  gs://fuchsia/{sdk_version}/bundles.json#terminal.qemu-arm64
* gs://fuchsia/{sdk_version}/bundles.json#core.x64-dfv2

*No need to fetch with `ffx product-bundle get ...`.
    """

  def testGetProductBundleUrlsMarksDesiredAsDownloaded(self):
    self._InitFFXRunWithProductBundleList()
    urls = update_product_bundles.get_product_bundle_urls()
    expected_urls = [{
        'url':
        'gs://fuchsia/10.20221114.2.1/bundles.json#workstation_eng.qemu-x64',
        'downloaded': False,
    }, {
        'url': ('gs://fuchsia/10.20221114.2.1/bundles.json#workstation_eng.'
                'chromebook-x64-dfv2'),
        'downloaded':
        False,
    }, {
        'url': ('gs://fuchsia/10.20221114.2.1/bundles.json#workstation_eng.'
                'chromebook-x64'),
        'downloaded':
        True,
    }, {
        'url': 'gs://fuchsia/10.20221114.2.1/bundles.json#terminal.qemu-x64',
        'downloaded': True,
    }, {
        'url': 'gs://fuchsia/10.20221114.2.1/bundles.json#terminal.qemu-arm64',
        'downloaded': False,
    }, {
        'url': 'gs://fuchsia/10.20221114.2.1/bundles.json#core.x64-dfv2',
        'downloaded': True,
    }]

    for i, url in enumerate(urls):
      self.assertEqual(url, expected_urls[i])

  @mock.patch('update_product_bundles.get_repositories')
  def testGetProductBundlesExtractsProductBundlesFromURLs(self, mock_get_repos):
    self._InitFFXRunWithProductBundleList()
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-x64'
    }, {
        'name': 'core.x64-dfv2'
    }]

    self.assertEqual(
        set(update_product_bundles.get_product_bundles()),
        set([
            'workstation_eng.chromebook-x64',
            'terminal.qemu-x64',
            'core.x64-dfv2',
        ]))

  @mock.patch('update_product_bundles.get_repositories')
  def testGetProductBundlesExtractsProductBundlesFromURLsFiltersMissingRepos(
      self, mock_get_repos):
    self._InitFFXRunWithProductBundleList()

    # This will be missing two repos from the bundle list:
    # core and terminal.qemu-x64
    # Additionally, workstation-eng != workstation_eng, but they will be treated
    # as the same product-bundle
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-arm64'
    }]

    self.assertEqual(update_product_bundles.get_product_bundles(),
                     ['workstation_eng.chromebook-x64'])
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=('product-bundle', 'remove', '-f', 'terminal.qemu-x64')),
        mock.call(cmd=('product-bundle', 'remove', '-f', 'core.x64-dfv2')),
    ],
                                    any_order=True)


if __name__ == '__main__':
  unittest.main()

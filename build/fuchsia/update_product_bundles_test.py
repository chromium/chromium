#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import os
import unittest
from unittest import mock

from parameterized import parameterized

import common
import ffx_session
import update_product_bundles


class TestUpdateProductBundles(unittest.TestCase):
  def testConvertToProductBundleDefaultsUnknownImage(self):
    self.assertEqual(
        update_product_bundles.convert_to_product_bundle(['unknown-image']),
        ['unknown-image'])

  def testConvertToProductBundleWarnsDeprecated(self):
    with self.assertLogs(level='WARNING') as logs:
      deprecated_images = [
          'qemu.arm64', 'qemu.x64', 'core.x64-dfv2-release',
          'workstation_eng.chromebook-x64-release'
      ]
      self.assertEqual(
          update_product_bundles.convert_to_product_bundle(deprecated_images), [
              'terminal.qemu-arm64', 'terminal.qemu-x64', 'core.x64-dfv2',
              'workstation_eng.chromebook-x64'
          ])
      for i, deprecated_image in enumerate(deprecated_images):
        self.assertIn(f'Image name {deprecated_image} has been deprecated',
                      logs.output[i])

  @mock.patch('builtins.open')
  @mock.patch('os.path.exists')
  def testGetHashFromSDK(self, mock_exists, mock_open):
    mock_open.return_value = io.StringIO(json.dumps({'id': 'foo-bar'}))
    mock_exists.return_value = True

    self.assertEqual(update_product_bundles.get_hash_from_sdk(), 'foo-bar')

    manifest_file = os.path.join(common.SDK_ROOT, 'meta', 'manifest.json')
    mock_exists.assert_called_once_with(manifest_file)
    mock_open.assert_called_once_with(manifest_file, 'r')

  @mock.patch('builtins.open')
  @mock.patch('os.path.exists')
  def testGetHashFromSDKRaisesErrorIfNoManifestExists(self, mock_exists,
                                                      mock_open):
    mock_exists.return_value = False

    self.assertRaises(RuntimeError, update_product_bundles.get_hash_from_sdk)

  def testRemoveRepositoriesRunsRemoveOnGivenRepos(self):
    ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)

    update_product_bundles.remove_repositories(['foo', 'bar', 'fizz', 'buzz'],
                                               ffx_runner)

    ffx_runner.run_ffx.assert_has_calls([
        mock.call(('repository', 'remove', 'foo'), check=True),
        mock.call(('repository', 'remove', 'bar'), check=True),
        mock.call(('repository', 'remove', 'fizz'), check=True),
        mock.call(('repository', 'remove', 'buzz'), check=True),
    ])

  @mock.patch('os.path.exists')
  @mock.patch('os.path.abspath')
  def testGetRepositoriesPrunesReposThatDoNotExist(self, mock_abspath,
                                                   mock_exists):
    with mock.patch('common.SDK_ROOT', 'some/path'):
      ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)
      ffx_runner.run_ffx.return_value = json.dumps([{
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

      self.assertEqual(update_product_bundles.get_repositories(ffx_runner),
                       [{
                           "name": "terminal.qemu-x64",
                           "spec": {
                               "type": "pm",
                               "path": "some/path/that/exists"
                           }
                       }])

      ffx_runner.run_ffx.assert_has_calls([
          mock.call(('--machine', 'json', 'repository', 'list')),
          mock.call(('repository', 'remove', 'workstation-eng.chromebook-x64'),
                    check=True)
      ])

  def testRemoveProductBundle(self):
    ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)

    update_product_bundles.remove_product_bundle('some-bundle-foo-bar',
                                                 ffx_runner)

    ffx_runner.run_ffx.assert_called_once_with(
        ('product-bundle', 'remove', '-f', 'some-bundle-foo-bar'))

  def _InitFFXRunWithProductBundleList(self, sdk_version='10.20221114.2.1'):
    ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)

    ffx_runner.run_ffx.return_value = f"""
  gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.qemu-x64
  gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.chromebook-x64-dfv2
* gs://fuchsia/{sdk_version}/bundles.json#workstation_eng.chromebook-x64
* gs://fuchsia/{sdk_version}/bundles.json#terminal.qemu-x64
  gs://fuchsia/{sdk_version}/bundles.json#terminal.qemu-arm64
* gs://fuchsia/{sdk_version}/bundles.json#core.x64-dfv2

*No need to fetch with `ffx product-bundle get ...`.
    """
    return ffx_runner

  def testGetProductBundleUrlsMarksDesiredAsDownloaded(self):
    urls = update_product_bundles.get_product_bundle_urls(
        self._InitFFXRunWithProductBundleList())
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
    ffx_runner = self._InitFFXRunWithProductBundleList()
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-x64'
    }, {
        'name': 'core.x64-dfv2'
    }]

    self.assertEqual(
        set(update_product_bundles.get_product_bundles(ffx_runner)),
        set([
            'workstation_eng.chromebook-x64',
            'terminal.qemu-x64',
            'core.x64-dfv2',
        ]))

  @mock.patch('update_product_bundles.get_repositories')
  def testGetProductBundlesExtractsProductBundlesFromURLsFiltersMissingRepos(
      self, mock_get_repos):
    ffx_runner = self._InitFFXRunWithProductBundleList()

    # This will be missing two repos from the bundle list:
    # core and terminal.qemu-x64
    # Additionally, workstation-eng != workstation_eng, but they will be treated
    # as the same product-bundle
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-arm64'
    }]

    self.assertEqual(update_product_bundles.get_product_bundles(ffx_runner),
                     ['workstation_eng.chromebook-x64'])
    ffx_runner.run_ffx.assert_has_calls([
        mock.call(('product-bundle', 'remove', '-f', 'terminal.qemu-x64')),
        mock.call(('product-bundle', 'remove', '-f', 'core.x64-dfv2')),
    ],
                                        any_order=True)

  @mock.patch('update_product_bundles.update_repositories_list')
  def testDownloadProductBundleUpdatesRepoListBeforeCall(
      self, mock_update_repo):
    ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)
    mock_sequence = mock.Mock()
    mock_sequence.attach_mock(mock_update_repo, 'update_repo_list')
    mock_sequence.attach_mock(ffx_runner.run_ffx, 'run_ffx')

    update_product_bundles.download_product_bundle('some-bundle', ffx_runner)

    mock_sequence.assert_has_calls([
        mock.call.update_repo_list(ffx_runner),
        mock.call.run_ffx(
            ('product-bundle', 'get', 'some-bundle', '--force-repo'))
    ])

  @mock.patch('update_product_bundles.get_product_bundle_urls')
  def testFilterProductBundleURLsRemovesBundlesWithoutGivenString(
      self, mock_get_urls):
    ffx_runner = mock.create_autospec(ffx_session.FfxRunner, instance=True)
    mock_get_urls.return_value = [
        {
            'url': 'some-url-has-buzz',
            'downloaded': True,
        },
        {
            'url': 'some-url-to-remove-has-foo',
            'downloaded': True,
        },
        {
            'url': 'some-url-to-not-remove-has-foo',
            'downloaded': False,
        },
    ]
    update_product_bundles.keep_product_bundles_by_sdk_version(
        'buzz', ffx_runner)
    ffx_runner.run_ffx.assert_called_once_with(
        ('product-bundle', 'remove', '-f', 'some-url-to-remove-has-foo'))

  @mock.patch('update_product_bundles.get_repositories')
  def testGetCurrentSignatureReturnsNoneIfNoProductBundles(
      self, mock_get_repos):
    ffx_runner = self._InitFFXRunWithProductBundleList()

    # Forces no product-bundles
    mock_get_repos.return_value = []

    # Mutes logs
    with self.assertLogs():
      self.assertIsNone(
          update_product_bundles.get_current_signature(ffx_runner))

  @mock.patch('update_product_bundles.get_repositories')
  def testGetCurrentSignatureParsesVersionCorrectly(self, mock_get_repos):
    ffx_runner = self._InitFFXRunWithProductBundleList()
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-x64'
    }]

    self.assertEqual('10.20221114.2.1',
                     update_product_bundles.get_current_signature(ffx_runner))

  @mock.patch('update_product_bundles.get_repositories')
  def testGetCurrentSignatureParsesCustomArtifactsCorrectlys(
      self, mock_get_repos):
    ffx_runner = self._InitFFXRunWithProductBundleList(sdk_version='51390009')
    mock_get_repos.return_value = [{
        'name': 'workstation-eng.chromebook-x64'
    }, {
        'name': 'terminal.qemu-x64'
    }]

    self.assertEqual('51390009',
                     update_product_bundles.get_current_signature(ffx_runner))


if __name__ == '__main__':
  unittest.main()

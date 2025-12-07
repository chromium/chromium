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

import update_product_bundles

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

import common
import compatible_utils


class TestUpdateProductBundles(unittest.TestCase):
  def setUp(self):
    # By default, test in attended mode.
    compatible_utils.force_running_attended()
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

  def testConvertToProductBundleRemovesReleaseSuffix(self):
    self.assertEqual(
        update_product_bundles.convert_to_products(
            ['smart_display_eng.astro-release']), ['smart_display_eng.astro'])

  def testConvertToProductBundleWarnsDeprecated(self):
    with self.assertLogs(level='WARNING') as logs:
      deprecated_images = [
          'qemu.arm64', 'qemu.x64', 'core.x64-dfv2-release',
          'workstation_eng.chromebook-x64-release'
      ]
      self.assertEqual(
          update_product_bundles.convert_to_products(deprecated_images), [
              'terminal.qemu-arm64', 'terminal.x64', 'core.x64-dfv2',
              'workstation_eng.chromebook-x64'
          ])
      for i, deprecated_image in enumerate(deprecated_images):
        self.assertIn(f'Image name {deprecated_image} has been deprecated',
                      logs.output[i])


  @mock.patch('common.run_ffx_command')
  def testRemoveRepositoriesRunsRemoveOnGivenRepos(self, ffx_mock):
    update_product_bundles.remove_repositories(['foo', 'bar', 'fizz', 'buzz'])

    ffx_mock.assert_has_calls([
        mock.call(cmd=('repository', 'remove', 'foo')),
        mock.call(cmd=('repository', 'remove', 'bar')),
        mock.call(cmd=('repository', 'remove', 'fizz')),
        mock.call(cmd=('repository', 'remove', 'buzz')),
    ])

  @mock.patch('os.path.exists')
  @mock.patch('os.path.abspath')
  def testGetRepositoriesPrunesReposThatDoNotExist(self, mock_abspath,
                                                   mock_exists):
    with mock.patch('common.SDK_ROOT', 'some/path'):
      self._ffx_mock.return_value.stdout = json.dumps([{
          "name": "terminal.x64",
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
          "name": "terminal.x64",
          "spec": {
              "type": "pm",
              "path": "some/path/that/exists"
          }
      }])

      self._ffx_mock.assert_has_calls([
          mock.call(cmd=('--machine', 'json', 'repository', 'list'),
                    capture_output=True),
          mock.call(cmd=('repository', 'remove',
                         'workstation-eng.chromebook-x64'))
      ])

  @mock.patch('common.make_clean_directory')
  @mock.patch('update_product_bundles.running_unattended', return_value=True)
  # Disallow reading sdk_override.
  @mock.patch('os.path.isfile', return_value=False)
  @mock.patch('update_product_bundles.internal_hash', return_value='1.1.1')
  def testLookupAndDownloadWithAuth(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    auth_file = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'get_auth_token.py'))
    self._ffx_mock.return_value.stdout = json.dumps({
        "name": "core.x64",
        "product_version": "17.20240106.2.1",
        "transfer_manifest_url": "http://download-url"
    })

    with mock.patch(
        'sys.argv',
        ['update_product_bundles.py', 'terminal.x64', '--internal']):
      update_product_bundles.main()
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=[
            '--machine', 'json', 'product', 'lookup', 'terminal.x64', '1.1.1',
            '--base-url', f'gs://fuchsia-sdk/development/1.1.1', '--auth',
            auth_file
        ],
                  capture_output=True),
        mock.call(cmd=[
            'product', 'download', 'http://download-url',
            os.path.join(common.INTERNAL_IMAGES_ROOT, 'terminal', 'x64'),
            '--auth', auth_file
        ])
    ])

  @mock.patch('common.make_clean_directory')
  @mock.patch('common.get_hash_from_sdk', return_value='2.2.2')
  @mock.patch('update_product_bundles.get_current_signature',
              return_value='2.2.2')
  @mock.patch('update_sdk.GetSDKOverrideGCSPath', return_value=None)
  def testIgnoreDownloadImagesWithSameHash(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    with mock.patch('sys.argv', ['update_product_bundles.py', 'terminal.x64']):
      update_product_bundles.main()
    self.assertFalse(self._ffx_mock.called)

  @mock.patch('common.make_clean_directory')
  @mock.patch('common.get_hash_from_sdk', return_value='2.2.2')
  @mock.patch('update_product_bundles.get_current_signature',
              return_value='0.0')
  @mock.patch('update_sdk.GetSDKOverrideGCSPath', return_value=None)
  def testDownloadImagesWithDifferentHash(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    self._ffx_mock.return_value.stdout = json.dumps({
        "name":
        "core.x64",
        "product_version":
        "17.20240106.2.1",
        "transfer_manifest_url":
        "http://download-url"
    })
    with mock.patch('sys.argv', ['update_product_bundles.py', 'terminal.x64']):
      update_product_bundles.main()
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=[
            '--machine', 'json', 'product', 'lookup', 'terminal.x64', '2.2.2',
            '--base-url', 'gs://fuchsia/development/2.2.2'
        ],
                  capture_output=True),
        mock.call(cmd=[
            'product', 'download', 'http://download-url',
            os.path.join(common.IMAGES_ROOT, 'terminal', 'x64')
        ])
    ])

  @mock.patch('common.make_clean_directory')
  @mock.patch('update_sdk.GetSDKOverrideGCSPath',
              return_value='gs://my-bucket/sdk')
  @mock.patch('common.get_hash_from_sdk', return_value='2.2.2')
  def testSDKOverrideForSDKImages(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    self._ffx_mock.return_value.stdout = json.dumps({
        "name":
        "core.x64",
        "product_version":
        "17.20240106.2.1",
        "transfer_manifest_url":
        "http://download-url"
    })
    with mock.patch('sys.argv', ['update_product_bundles.py', 'terminal.x64']):
      update_product_bundles.main()
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=[
            '--machine', 'json', 'product', 'lookup', 'terminal.x64', '2.2.2',
            '--base-url', 'gs://my-bucket'
        ],
                  capture_output=True),
        mock.call(cmd=[
            'product', 'download', 'http://download-url',
            os.path.join(common.IMAGES_ROOT, 'terminal', 'x64')
        ])
    ])

  @mock.patch('common.make_clean_directory')
  @mock.patch('update_product_bundles.internal_hash', return_value='1.1.1')
  @mock.patch('update_product_bundles.get_current_signature',
              return_value='1.1.1')
  def testIgnoreDownloadInternalImagesWithSameHash(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    with mock.patch(
        'sys.argv',
        ['update_product_bundles.py', 'terminal.x64', '--internal']):
      update_product_bundles.main()
    self.assertFalse(self._ffx_mock.called)

  @mock.patch('common.make_clean_directory')
  @mock.patch('update_product_bundles.get_current_signature',
              return_value='0.0')
  @mock.patch('update_product_bundles.internal_hash', return_value='1.1.1')
  def testDownloadInternalImagesWithDifferentHash(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    self._ffx_mock.return_value.stdout = json.dumps({
        "name":
        "core.x64",
        "product_version":
        "17.20240106.2.1",
        "transfer_manifest_url":
        "http://download-url"
    })
    with mock.patch(
        'sys.argv',
        ['update_product_bundles.py', 'terminal.x64', '--internal']):
      update_product_bundles.main()
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=[
            '--machine', 'json', 'product', 'lookup', 'terminal.x64', '1.1.1',
            '--base-url', 'gs://fuchsia-sdk/development/1.1.1'
        ],
                  capture_output=True),
        mock.call(cmd=[
            'product', 'download', 'http://download-url',
            os.path.join(common.INTERNAL_IMAGES_ROOT, 'terminal', 'x64')
        ])
    ])

  @mock.patch('common.make_clean_directory')
  @mock.patch('update_sdk.GetSDKOverrideGCSPath',
              return_value='gs://my-bucket/sdk')
  @mock.patch('update_product_bundles.internal_hash', return_value='1.1.1')
  def testIgnoreSDKOverrideForInternalImages(self, *_):
    try:
      common.get_host_os()
    except:
      # Ignore unsupported platforms. common.get_host_os used in
      # update_product_bundles.main throws an unsupported exception.
      return
    self._ffx_mock.return_value.stdout = json.dumps({
        "name":
        "core.x64",
        "product_version":
        "17.20240106.2.1",
        "transfer_manifest_url":
        "http://download-url"
    })
    with mock.patch(
        'sys.argv',
        ['update_product_bundles.py', 'terminal.x64', '--internal']):
      update_product_bundles.main()
    self._ffx_mock.assert_has_calls([
        mock.call(cmd=[
            '--machine', 'json', 'product', 'lookup', 'terminal.x64', '1.1.1',
            '--base-url', 'gs://fuchsia-sdk/development/1.1.1'
        ],
                  capture_output=True),
        mock.call(cmd=[
            'product', 'download', 'http://download-url',
            os.path.join(common.INTERNAL_IMAGES_ROOT, 'terminal', 'x64')
        ])
    ])


if __name__ == '__main__':
  unittest.main()

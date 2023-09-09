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
                    capture_output=True,
                    check=True),
          mock.call(cmd=('repository', 'remove',
                         'workstation-eng.chromebook-x64'),
                    check=True)
      ])


if __name__ == '__main__':
  unittest.main()

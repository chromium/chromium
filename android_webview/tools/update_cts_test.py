#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest
from mock import patch  # pylint: disable=import-error

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, 'third_party',
        'catapult', 'common', 'py_utils'))
# pylint: disable=wrong-import-position,import-error
from py_utils import tempfile_ext

import update_cts
import cts_utils

from cts_utils_test import CONFIG_DATA


class UpdateCTSTest(unittest.TestCase):
  """Unittests for update_cts.py."""

  @patch('devil.utils.cmd_helper.GetCmdOutput')
  def testUpdateCtsConfigFileOrigins(self, cmd_mock):
    with tempfile_ext.NamedTemporaryDirectory() as repoRoot:

      cmd_mock.return_value = """
      hash        refs/tags/platform-1.0_r6
      hash        refs/tags/platform-1.0_r7
      hash        refs/tags/platform-1.0_r9
      hash        refs/tags/platform-2.0_r2
      hash        refs/tags/platform-2.0_r3
      """

      expected_config_file = json.loads(CONFIG_DATA['json'])
      expected_config_file['platform1']['arch']['arch1'][
          'unzip_dir'] = 'arch1/path/platform1_r9'
      expected_config_file['platform1']['arch']['arch2'][
          'unzip_dir'] = 'arch1/path/platform1_r9'
      expected_config_file['platform2']['arch']['arch1'][
          'unzip_dir'] = 'arch1/path/platform2_r3'
      expected_config_file['platform2']['arch']['arch2'][
          'unzip_dir'] = 'arch1/path/platform2_r3'


      config_path = os.path.join(repoRoot, cts_utils.TOOLS_DIR,
                                 cts_utils.CONFIG_FILE)
      os.makedirs(os.path.dirname(config_path))
      with open(config_path, 'w') as f:
        f.write(CONFIG_DATA['json'])

      cts_updater = update_cts.UpdateCTS(repoRoot)
      cts_updater.update_cts_download_origins_cmd()

      with open(config_path) as f:
        actual_config_file = json.load(f)

      self.assertEqual(expected_config_file, actual_config_file)


if __name__ == '__main__':
  unittest.main()

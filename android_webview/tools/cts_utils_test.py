#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import unittest

import cts_utils

CONFIG_DATA = {}
CONFIG_DATA['json'] = """{
  "platform1": {
    "git": {
      "tag_prefix": "platform-1.0"
    },
    "arch": {
      "arch1": {
        "filename": "arch1/platform1/file1.zip",
        "_origin": "https://a1.p1/f1.zip",
        "unzip_dir": "arch1/path/platform1_r1"
      },
      "arch2": {
        "filename": "arch2/platform1/file3.zip",
        "_origin": "https://a2.p1/f3.zip",
        "unzip_dir": "arch1/path/platform1_r1"
      }
    },
    "test_runs": [
      {
        "apk": "p1/test.apk"
      }
    ]
  },
  "platform2": {
    "git": {
      "tag_prefix": "platform-2.0"
    },
    "arch": {
      "arch1": {
        "filename": "arch1/platform2/file2.zip",
        "_origin": "https://a1.p2/f2.zip",
        "unzip_dir": "arch1/path/platform2_r1"
      },
      "arch2": {
        "filename": "arch2/platform2/file4.zip",
        "_origin": "https://a2.p2/f4.zip",
        "unzip_dir": "arch1/path/platform2_r1"
      }
    },
    "test_runs": [
      {
        "apk": "p2/test1.apk",
        "additional_apks": [
          {
            "apk": "p2/additional_apk_a_1.apk"
          }
        ]
      },
      {
        "apk": "p2/test2.apk",
        "additional_apks": [
          {
            "apk": "p2/additional_apk_b_1.apk",
            "forced_queryable": true
          },
          {
            "apk": "p2/additional_apk_b_2.apk"
          }
        ]
      }
    ]
  }
}
"""
CONFIG_DATA['origin11'] = 'https://a1.p1/f1.zip'
CONFIG_DATA['base11'] = 'f1.zip'
CONFIG_DATA['file11'] = 'arch1/platform1/file1.zip'
CONFIG_DATA['origin12'] = 'https://a2.p1/f3.zip'
CONFIG_DATA['base12'] = 'f3.zip'
CONFIG_DATA['file12'] = 'arch2/platform1/file3.zip'
CONFIG_DATA['apk1'] = 'p1/test.apk'
CONFIG_DATA['origin21'] = 'https://a1.p2/f2.zip'
CONFIG_DATA['base21'] = 'f2.zip'
CONFIG_DATA['file21'] = 'arch1/platform2/file2.zip'
CONFIG_DATA['origin22'] = 'https://a2.p2/f4.zip'
CONFIG_DATA['base22'] = 'f4.zip'
CONFIG_DATA['file22'] = 'arch2/platform2/file4.zip'
CONFIG_DATA['apk2a'] = 'p2/test1.apk'
CONFIG_DATA['apk2b'] = 'p2/test2.apk'


class CTSUtilsTest(unittest.TestCase):
  """Unittests for the cts_utils.py."""

  def testCTSConfigSanity(self):
    cts_config = cts_utils.CTSConfig()
    platforms = cts_config.get_platforms()
    self.assertTrue(platforms)
    platform = platforms[0]
    archs = cts_config.get_archs(platform)
    self.assertTrue(archs)

  @unittest.skipIf(os.name == "nt", "Opening NamedTemporaryFile by name "
                   "doesn't work in Windows.")
  def testCTSConfig(self):
    with tempfile.NamedTemporaryFile('w+t') as configFile:
      configFile.writelines(CONFIG_DATA['json'])
      configFile.flush()
      cts_config = cts_utils.CTSConfig(configFile.name)
    self.assertEqual(['platform1', 'platform2'], cts_config.get_platforms())
    self.assertEqual(['arch1', 'arch2'], cts_config.get_archs('platform1'))
    self.assertEqual(['arch1', 'arch2'], cts_config.get_archs('platform2'))

  def testChromiumRepoHelper(self):
    helper = cts_utils.ChromiumRepoHelper(root_dir='.')
    root_dir = os.path.abspath('.')
    self.assertEqual(os.path.join(root_dir, 'a', 'b'), helper.rebase('a', 'b'))


if __name__ == '__main__':
  unittest.main()

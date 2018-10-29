#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

file_dir_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(file_dir_path, '..', '..', '..'))
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

class ShellApkVersion(unittest.TestCase):
   UPDATE_CURRENT_VERSION_MESSAGE = (
       'current_shell_apk_version in '
       'shell_apk/current_version/current_version.gni needs to updated due to '
       'changes in:')

   def makeMockAffectedFiles(self, file_names):
     mock_files = []
     for file_name in file_names:
       mock_files.append(
           MockAffectedFile(file_name, ['new_content'], action='A'))
     return mock_files

   def testCheckWamMintTriggerRule(self):
     COMMON_SRC_FILE_PATH = (
         'libs/common/src/org/chromium/webapk/lib/common/A.java')
     COMMON_JUNIT_FILE_PATH = (
         'libs/common/junit/src/org/chromium/webapk/lib/common/B.java')
     SHELL_APK_SRC_FILE_PATH = (
         'shell_apk/src/org/chromium/webapk/shell_apk/C.java')
     SHELL_APK_JUNIT_FILE_PATH = (
         'shell_apk/junit/src/org/chromium/webapk/shell_apk/D.java')
     changed_java_file_paths = [
         COMMON_SRC_FILE_PATH, COMMON_JUNIT_FILE_PATH, SHELL_APK_SRC_FILE_PATH,
         SHELL_APK_JUNIT_FILE_PATH
     ]

     SHELL_APK_RES_FILE_PATH = 'shell_apk/res/mipmap-xxxxxxhdpi/app_icon.png'
     CURRENT_VERSION_FILE_PATH = 'shell_apk/current_version/current_version.gni'

     # template_shell_apk_version not updated. There should be a warning about
     # template_shell_apk_version needing to be updated.
     input_api = MockInputApi()
     input_api.files = self.makeMockAffectedFiles(
         changed_java_file_paths + [SHELL_APK_RES_FILE_PATH])
     input_api.files += [
         MockAffectedFile(CURRENT_VERSION_FILE_PATH, 'variable=O',
                          'variable=N', action='M')
     ]
     warnings = PRESUBMIT._CheckCurrentVersionIncreaseRule(input_api,
                                                           MockOutputApi())
     self.assertEqual(1, len(warnings))
     self.assertEqual(self.UPDATE_CURRENT_VERSION_MESSAGE, warnings[0].message)
     self.assertEqual([COMMON_SRC_FILE_PATH, SHELL_APK_SRC_FILE_PATH,
                       SHELL_APK_RES_FILE_PATH],
                      warnings[0].items)

     # template_shell_apk_version updated. There should be no warnings.
     input_api.files = self.makeMockAffectedFiles(
         changed_java_file_paths + [SHELL_APK_RES_FILE_PATH])
     input_api.files += [
         MockAffectedFile(CURRENT_VERSION_FILE_PATH,
                          ['current_shell_apk_version=1'],
                          ['current_shell_apk_version=2'], action='M')
     ]
     warnings = PRESUBMIT._CheckCurrentVersionIncreaseRule(input_api,
                                                           MockOutputApi())
     self.assertEqual([], warnings)

if __name__ == '__main__':
  unittest.main()

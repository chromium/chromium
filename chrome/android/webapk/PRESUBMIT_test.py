#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
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

# Mocks os.walk()
class MockOsWalkFileSystem(object):
    def __init__(self, file_paths):
        self.file_paths = file_paths

    def walk(self, top):
        if not top.endswith('/'):
            top += '/'

        files = []
        dirs = []
        for f in self.file_paths:
            if f.startswith(top):
                remaining = f[len(top):]
                slash_index = remaining.find('/')
                if slash_index >= 0:
                    dir_name = remaining[:slash_index]
                    if not dir_name in dirs:
                        dirs.append(dir_name)
                else:
                    files.append(remaining)

            yield top[:-1], dirs, files

        for name in dirs:
            for result in self.walk(top + name):
                yield result


def make_mock_affected_files(file_names, extra_files=[]):
    D = 'chrome/android/webapk'
    ret = []
    for file_name in file_names:
        ret.append(
            MockAffectedFile(f'{D}/{file_name}', ['new_content'], action='A'))
    for mock_file in extra_files:
        ret.append(
            MockAffectedFile(f'{D}/{mock_file.LocalPath()}',
                             mock_file.NewContents(),
                             action=mock_file.Action()))

    return ret


class ShellApkVersion(unittest.TestCase):
    UPDATE_CURRENT_VERSION_MESSAGE = (
        'current_shell_apk_version in '
        'shell_apk/current_version/current_version.gni needs to updated due to '
        'changes in:')

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

        # template_shell_apk_version not updated. There should be an error about
        # template_shell_apk_version needing to be updated.
        input_api = MockInputApi()
        input_api.InitFiles(
            make_mock_affected_files(
                changed_java_file_paths + [SHELL_APK_RES_FILE_PATH], [
                    MockAffectedFile(CURRENT_VERSION_FILE_PATH,
                                     'variable=O',
                                     'variable=N',
                                     action='M')
                ]))
        errors = PRESUBMIT._CheckCurrentVersionIncreaseRule(input_api,
                                                            MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertEqual(self.UPDATE_CURRENT_VERSION_MESSAGE, errors[0].message)
        self.assertEqual([COMMON_SRC_FILE_PATH, SHELL_APK_SRC_FILE_PATH,
                          SHELL_APK_RES_FILE_PATH],
                         errors[0].items)

        # template_shell_apk_version updated. There should be no errors.
        input_api.InitFiles(
            make_mock_affected_files(
                changed_java_file_paths + [SHELL_APK_RES_FILE_PATH], [
                    MockAffectedFile(CURRENT_VERSION_FILE_PATH,
                                     ['current_shell_apk_version=1'],
                                     ['current_shell_apk_version=2'],
                                     action='M')
                ]))
        errors = PRESUBMIT._CheckCurrentVersionIncreaseRule(input_api,
                                                            MockOutputApi())
        self.assertEqual([], errors)


class OverlappingResourceFileNames(unittest.TestCase):
    RESOURCES_SHOULD_HAVE_DIFFERENT_FILE_NAMES_MESSAGE = (
        'Resources in different top level res/ directories [\'shell_apk/res\', '
        '\'shell_apk/res_template\', \'libs/common/res_splash\'] should have '
        'different names:')

    def testAddFileSameNameWithinResDirectory(self):
        # Files within a res/ directory can have same file name.
        MOCK_FILE_SYSTEM_FILES = ['shell_apk/res/values/colors.xml',
                                  'libs/common/res_splash/values/dimens.xml']
        input_api = MockInputApi()
        input_api.os_walk = MockOsWalkFileSystem(MOCK_FILE_SYSTEM_FILES).walk

        input_api.InitFiles(
            make_mock_affected_files(['shell_apk/res/values-v22/values.xml']))
        errors = PRESUBMIT._CheckNoOverlappingFileNamesInResourceDirsRule(
            input_api, MockOutputApi())
        self.assertEqual(0, len(errors))

    def testAddFileSameNameAcrossResDirectories(self):
        # Adding a file to a res/ directory with the same file name as a file in a
        # different res/ directory is illegal.
        MOCK_FILE_SYSTEM_FILES = ['shell_apk/res/values/colors.xml',
                                  'libs/common/res_splash/values/dimens.xml']
        input_api = MockInputApi()
        input_api.os_walk = MockOsWalkFileSystem(MOCK_FILE_SYSTEM_FILES).walk
        input_api.InitFiles(
            make_mock_affected_files([
                'shell_apk/res/values-v17/dimens.xml',
                'libs/common/res_splash/values-v22/colors.xml'
            ]))
        errors = PRESUBMIT._CheckNoOverlappingFileNamesInResourceDirsRule(
            input_api, MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertEqual(self.RESOURCES_SHOULD_HAVE_DIFFERENT_FILE_NAMES_MESSAGE,
                         errors[0].message)
        errors[0].items.sort()
        self.assertEqual(['colors.xml', 'dimens.xml'], errors[0].items)

    def testAddTwoFilesWithSameNameDifferentResDirectories(self):
        # Adding two files with the same file name but in different res/
        # directories is illegal.
        MOCK_FILE_SYSTEM_FILES = ['shell_apk/res/values/colors.xml',
                                  'libs/common/res_splash/values/dimens.xml']
        input_api = MockInputApi()
        input_api.os_walk = MockOsWalkFileSystem(MOCK_FILE_SYSTEM_FILES).walk
        input_api.InitFiles(
            make_mock_affected_files([
                'shell_apk/res/values/values.xml',
                'libs/common/res_splash/values-v22/values.xml'
            ]))
        errors = PRESUBMIT._CheckNoOverlappingFileNamesInResourceDirsRule(
            input_api, MockOutputApi())
        self.assertEqual(1, len(errors))
        self.assertEqual(self.RESOURCES_SHOULD_HAVE_DIFFERENT_FILE_NAMES_MESSAGE,
                         errors[0].message)
        self.assertEqual(['values.xml'], errors[0].items)

if __name__ == '__main__':
    unittest.main()

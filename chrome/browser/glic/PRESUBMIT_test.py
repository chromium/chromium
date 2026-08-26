# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.util
import os
import sys
import tempfile
import unittest

import PRESUBMIT

# Append chrome source root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
import PRESUBMIT_test_mocks


class CheckHeaderOrderingTest(unittest.TestCase):

    def testNoAffectedFiles(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testNonCppFilesEdited(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/BUILD.gn',
                ['# Some build file content'],
            ),
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/tools/sort_headers.py',
                ['# Some python content'],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testSortedCppFile(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.files = [
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/actor/'
                'glic_actor_action_execution_browsertest.cc',
                ['// Copyright 2026'],
            ),
        ]
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testUnsortedCppFile(self):
        # Create a temporary C++ file with unsorted headers inside
        # chrome/browser/glic.
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        tmp_file_path = os.path.join(glic_dir, 'test_unsorted_header_file.cc')
        rel_path = os.path.relpath(tmp_file_path, repo_root)
        unix_rel_path = rel_path.replace('\\', '/')

        unsorted_content = ('#include <vector>\n'
                            '#include "base/logging.h"\n'
                            '#include <string>\n')
        with open(tmp_file_path, 'w', encoding='utf-8') as f:
            f.write(unsorted_content)

        try:
            mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
            mock_input_api.presubmit_local_path = glic_dir
            mock_input_api.InitFiles([
                PRESUBMIT_test_mocks.MockFile(
                    rel_path,
                    unsorted_content.splitlines(),
                ),
            ])
            mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
            results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                    mock_output_api)
            self.assertEqual(len(results), 1)
            self.assertEqual(results[0].type, 'error')
            self.assertIn('not properly sorted', results[0].message)
            self.assertIn(
                'python3 chrome/browser/glic/tools/sort_headers.py '
                f'{unix_rel_path}', results[0].message)
            self.assertIn(unix_rel_path, results[0].items)
        finally:
            if os.path.exists(tmp_file_path):
                os.remove(tmp_file_path)

    def testCppFileOutsideGlicIgnored(self):
        # Create a temporary C++ file outside chrome/browser/glic with
        # unsorted headers.
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        notes_dir = os.path.join(repo_root, '.notes')
        os.makedirs(notes_dir, exist_ok=True)
        tmp_file_path = os.path.join(notes_dir, 'outside_glic_file.cc')
        rel_path = os.path.relpath(tmp_file_path, repo_root)

        unsorted_content = ('#include <vector>\n'
                            '#include "base/logging.h"\n'
                            '#include <string>\n')
        with open(tmp_file_path, 'w', encoding='utf-8') as f:
            f.write(unsorted_content)

        try:
            mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
            mock_input_api.presubmit_local_path = os.path.join(
                repo_root, 'chrome', 'browser', 'glic')
            mock_input_api.InitFiles([
                PRESUBMIT_test_mocks.MockFile(
                    rel_path,
                    unsorted_content.splitlines(),
                ),
            ])
            mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
            results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                    mock_output_api)
            self.assertEqual(results, [])
        finally:
            if os.path.exists(tmp_file_path):
                os.remove(tmp_file_path)

    def testCppFileWithoutIncludeChangesIgnored(self):
        # Create a temporary C++ file with unsorted headers on disk, but
        # simulate modifying only non-include lines.
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        tmp_file_path = os.path.join(glic_dir, 'test_no_include_change.cc')
        rel_path = os.path.relpath(tmp_file_path, repo_root)

        unsorted_content = ('#include <vector>\n'
                            '#include "base/logging.h"\n'
                            '#include <string>\n')
        with open(tmp_file_path, 'w', encoding='utf-8') as f:
            f.write(unsorted_content)

        try:
            mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
            mock_input_api.presubmit_local_path = glic_dir
            mock_input_api.InitFiles([
                PRESUBMIT_test_mocks.MockFile(
                    rel_path,
                    ['void SomeFunction() {}'],
                ),
            ])
            mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
            results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                    mock_output_api)
            self.assertEqual(results, [])
        finally:
            if os.path.exists(tmp_file_path):
                os.remove(tmp_file_path)

    def testIndentedIncludeTrigger(self):
        # Create a temporary C++ file with unsorted indented headers.
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        tmp_file_path = os.path.join(glic_dir, 'test_indented_include.cc')
        rel_path = os.path.relpath(tmp_file_path, repo_root)
        unix_rel_path = rel_path.replace('\\', '/')

        unsorted_content = ('#   include <vector>\n'
                            '#   include "base/logging.h"\n'
                            '#   include <string>\n')
        with open(tmp_file_path, 'w', encoding='utf-8') as f:
            f.write(unsorted_content)

        try:
            mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
            mock_input_api.presubmit_local_path = glic_dir
            mock_input_api.InitFiles([
                PRESUBMIT_test_mocks.MockFile(
                    rel_path,
                    unsorted_content.splitlines(),
                ),
            ])
            mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
            results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                    mock_output_api)
            self.assertEqual(len(results), 1)
            self.assertEqual(results[0].type, 'error')
            self.assertIn('not properly sorted', results[0].message)
            self.assertIn(unix_rel_path, results[0].items)
        finally:
            if os.path.exists(tmp_file_path):
                os.remove(tmp_file_path)


class CheckGlicApiTestRegistrationTest(unittest.TestCase):

    def testNoAffectedFiles(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testValidGlicTestPair(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = glic_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/host/glic_focus_browsertest.cc',
                ['// Copyright 2026'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testValidGlicTestPairOutsideGlicDir(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = glic_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/sharing/glic_experimental_triggering/'
                'glic_experimental_triggering_message_handler_browsertest.cc',
                ['// Copyright 2026'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testCheckerScriptItselfAffected(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = glic_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/tools/'
                'check_glic_api_test_registration.py',
                ['# Copyright 2026'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testMissingTestRegistration(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_mock = os.path.join(temp_dir, 'repo')
            glic_dir = os.path.join(repo_mock, 'chrome', 'browser', 'glic')
            webui_dir = os.path.join(repo_mock, 'chrome', 'test', 'data',
                                     'webui', 'glic', 'browser_tests')
            os.makedirs(glic_dir, exist_ok=True)
            os.makedirs(webui_dir, exist_ok=True)

            tmp_cc = os.path.join(glic_dir, 'tmp_mock_browsertest.cc')
            tmp_ts = os.path.join(webui_dir, 'tmp_mock_browsertest.ts')

            cc_content = (
                '#include "chrome/browser/glic/test_support/'
                'glic_api_test.h"\n'
                'class TmpMockBrowserTest : public GlicApiBrowserTest {\n'
                ' public:\n'
                '  TmpMockBrowserTest() :\n'
                '      GlicApiBrowserTest('
                'GlicTestJsPath("./tmp_mock_browsertest.js")) {}\n'
                '};\n'
                'IN_PROC_BROWSER_TEST_F(TmpMockBrowserTest, '
                'testRegistered) {}\n')

            ts_content = ('class TmpMockTest extends ApiTestFixtureBase {\n'
                          '  async testRegistered() {}\n'
                          '  async testUnregisteredInCpp() {}\n'
                          '}\n')

            with open(tmp_cc, 'w', encoding='utf-8') as f:
                f.write(cc_content)
            with open(tmp_ts, 'w', encoding='utf-8') as f:
                f.write(ts_content)

            real_glic_dir = os.path.join(PRESUBMIT_test_mocks._REPO_ROOT,
                                         'chrome', 'browser', 'glic')
            mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
            mock_input_api.presubmit_local_path = real_glic_dir
            mock_input_api.change.RepositoryRoot = lambda: repo_mock
            mock_input_api.InitFiles([
                PRESUBMIT_test_mocks.MockFile(
                    'chrome/browser/glic/tmp_mock_browsertest.cc',
                    cc_content.splitlines(),
                ),
            ])
            mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
            results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                    mock_output_api)
            self.assertEqual(len(results), 1)
            self.assertEqual(results[0].type, 'error')
            self.assertIn('Glic API test registration check failed',
                          results[0].message)
            self.assertIn('testUnregisteredInCpp', results[0].message)

    def testWebUiGlicPresubmitTriggersOnTsFile(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        webui_dir = os.path.join(repo_root, 'chrome', 'test', 'data', 'webui',
                                 'glic')
        presubmit_path = os.path.join(webui_dir, 'PRESUBMIT.py')
        spec = importlib.util.spec_from_file_location('webui_glic_presubmit',
                                                      presubmit_path)
        webui_presubmit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(webui_presubmit)

        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = webui_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/test/data/webui/glic/browser_tests/'
                'glic_focus_browsertest.ts',
                ['// Copyright 2026'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = webui_presubmit.CheckChangeOnUpload(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(results, [])

    def testWebUiGlicPresubmitIgnoresSupportFile(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        webui_dir = os.path.join(repo_root, 'chrome', 'test', 'data', 'webui',
                                 'glic')
        presubmit_path = os.path.join(webui_dir, 'PRESUBMIT.py')
        spec = importlib.util.spec_from_file_location('webui_glic_presubmit',
                                                      presubmit_path)
        webui_presubmit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(webui_presubmit)

        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = webui_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/test/data/webui/glic/browser_tests/browser_test_base.ts',
                ['export function testStepper() {}'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = webui_presubmit.CheckChangeOnUpload(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(results, [])

    def testGlicPresubmitIgnoresNonTestCppFile(self):
        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/glic/glic_keyed_service.cc',
                ['void Service() {}'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT.CheckChangeOnUpload(mock_input_api,
                                                mock_output_api)
        self.assertEqual(results, [])

    def testSharingPresubmitTriggersOnBrowserTest(self):
        repo_root = PRESUBMIT_test_mocks._REPO_ROOT
        sharing_dir = os.path.join(repo_root, 'chrome', 'browser', 'sharing',
                                   'glic_experimental_triggering')
        presubmit_path = os.path.join(sharing_dir, 'PRESUBMIT.py')
        spec = importlib.util.spec_from_file_location('sharing_glic_presubmit',
                                                      presubmit_path)
        sharing_presubmit = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(sharing_presubmit)

        mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
        mock_input_api.presubmit_local_path = sharing_dir
        mock_input_api.InitFiles([
            PRESUBMIT_test_mocks.MockFile(
                'chrome/browser/sharing/glic_experimental_triggering/'
                'glic_experimental_triggering_message_handler_browsertest.cc',
                ['// Copyright 2026'],
            ),
        ])
        mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
        results = sharing_presubmit.CheckChangeOnUpload(
            mock_input_api, mock_output_api)
        self.assertEqual(results, [])


# Include tool unit tests so they execute with PRESUBMIT_test.py
sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tools'))
from check_glic_api_test_registration_test import (
    CheckGlicApiTestRegistrationCliAndRepoTest,
    CheckGlicApiTestRegistrationCppExtractorTest,
    CheckGlicApiTestRegistrationCppHandlingTest,
    CheckGlicApiTestRegistrationFileTypeTest,
    CheckGlicApiTestRegistrationPairMatchingTest,
    CheckGlicApiTestRegistrationRepoRootTest,
    CheckGlicApiTestRegistrationTsHandlingTest,
    CheckGlicApiTestRegistrationTypeScriptExtractorTest,
)
from sort_headers_test import SortHeadersTest

if __name__ == '__main__':
    unittest.main()

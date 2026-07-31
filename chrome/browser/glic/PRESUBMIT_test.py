# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
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
            self.assertEqual(results[0].type, 'warning')
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
            self.assertEqual(results[0].type, 'warning')
            self.assertIn('not properly sorted', results[0].message)
            self.assertIn(unix_rel_path, results[0].items)
        finally:
            if os.path.exists(tmp_file_path):
                os.remove(tmp_file_path)


if __name__ == '__main__':
    unittest.main()

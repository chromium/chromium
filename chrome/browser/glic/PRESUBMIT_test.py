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
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
  )
)
import PRESUBMIT_test_mocks


class CheckHeaderOrderingTest(unittest.TestCase):
  def testNoAffectedFiles(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
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
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testSortedCppFile(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockFile(
        'chrome/browser/glic/actor/glic_actor_action_execution_browsertest.cc',
        ['// Copyright 2026'],
      ),
    ]
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testUnsortedCppFile(self):
    # Create a temporary C++ file with unsorted headers inside
    # chrome/browser/glic.
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
    tmp_file_path = os.path.join(glic_dir, 'test_unsorted_header_file.cc')
    rel_path = os.path.relpath(tmp_file_path, repo_root)
    unix_rel_path = rel_path.replace('\\', '/')

    unsorted_content = (
      '#include <vector>\n#include "base/logging.h"\n#include <string>\n'
    )
    with open(tmp_file_path, 'w', encoding='utf-8') as f:
      f.write(unsorted_content)

    try:
      mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
      mock_input_api.presubmit_local_path = glic_dir
      mock_input_api.InitFiles(
        [
          PRESUBMIT_test_mocks.MockFile(
            rel_path,
            unsorted_content.splitlines(),
          ),
        ]
      )
      mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
      results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
      self.assertEqual(len(results), 1)
      self.assertEqual(results[0].type, 'error')
      self.assertIn('not properly sorted', results[0].message)
      self.assertIn(
        f'python3 chrome/browser/glic/tools/sort_headers.py {unix_rel_path}',
        results[0].message,
      )
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

    unsorted_content = (
      '#include <vector>\n#include "base/logging.h"\n#include <string>\n'
    )
    with open(tmp_file_path, 'w', encoding='utf-8') as f:
      f.write(unsorted_content)

    try:
      mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
      mock_input_api.presubmit_local_path = os.path.join(
        repo_root, 'chrome', 'browser', 'glic'
      )
      mock_input_api.InitFiles(
        [
          PRESUBMIT_test_mocks.MockFile(
            rel_path,
            unsorted_content.splitlines(),
          ),
        ]
      )
      mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
      results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
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

    unsorted_content = (
      '#include <vector>\n#include "base/logging.h"\n#include <string>\n'
    )
    with open(tmp_file_path, 'w', encoding='utf-8') as f:
      f.write(unsorted_content)

    try:
      mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
      mock_input_api.presubmit_local_path = glic_dir
      mock_input_api.InitFiles(
        [
          PRESUBMIT_test_mocks.MockFile(
            rel_path,
            ['void SomeFunction() {}'],
          ),
        ]
      )
      mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
      results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
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

    unsorted_content = (
      '#   include <vector>\n'
      '#   include "base/logging.h"\n'
      '#   include <string>\n'
    )
    with open(tmp_file_path, 'w', encoding='utf-8') as f:
      f.write(unsorted_content)

    try:
      mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
      mock_input_api.presubmit_local_path = glic_dir
      mock_input_api.InitFiles(
        [
          PRESUBMIT_test_mocks.MockFile(
            rel_path,
            unsorted_content.splitlines(),
          ),
        ]
      )
      mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
      results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
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
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testValidGlicTestPair(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = glic_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/host/glic_focus_browsertest.cc',
          ['// Copyright 2026'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testValidGlicTestPairOutsideGlicDir(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = glic_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/sharing/glic_experimental_triggering/'
          'glic_experimental_triggering_message_handler_browsertest.cc',
          ['// Copyright 2026'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testCheckerScriptItselfAffected(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    glic_dir = os.path.join(repo_root, 'chrome', 'browser', 'glic')
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = glic_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/tools/check_glic_api_test_registration.py',
          ['# Copyright 2026'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testMissingTestRegistration(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      repo_mock = os.path.join(temp_dir, 'repo')
      glic_dir = os.path.join(repo_mock, 'chrome', 'browser', 'glic')
      webui_dir = os.path.join(
        repo_mock, 'chrome', 'test', 'data', 'webui', 'glic', 'browser_tests'
      )
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
        'testRegistered) {}\n'
      )

      ts_content = (
        'class TmpMockTest extends ApiTestFixtureBase {\n'
        '  async testRegistered() {}\n'
        '  async testUnregisteredInCpp() {}\n'
        '}\n'
      )

      with open(tmp_cc, 'w', encoding='utf-8') as f:
        f.write(cc_content)
      with open(tmp_ts, 'w', encoding='utf-8') as f:
        f.write(ts_content)

      real_glic_dir = os.path.join(
        PRESUBMIT_test_mocks._REPO_ROOT, 'chrome', 'browser', 'glic'
      )
      mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
      mock_input_api.presubmit_local_path = real_glic_dir
      mock_input_api.change.RepositoryRoot = lambda: repo_mock
      mock_input_api.InitFiles(
        [
          PRESUBMIT_test_mocks.MockFile(
            'chrome/browser/glic/tmp_mock_browsertest.cc',
            cc_content.splitlines(),
          ),
        ]
      )
      mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
      results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
      self.assertEqual(len(results), 1)
      self.assertEqual(results[0].type, 'error')
      self.assertIn(
        'Glic API test registration check failed', results[0].message
      )
      self.assertIn('testUnregisteredInCpp', results[0].message)

  def testWebUiGlicPresubmitTriggersOnTsFile(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    webui_dir = os.path.join(
      repo_root, 'chrome', 'test', 'data', 'webui', 'glic'
    )
    presubmit_path = os.path.join(webui_dir, 'PRESUBMIT.py')
    spec = importlib.util.spec_from_file_location(
      'webui_glic_presubmit', presubmit_path
    )
    webui_presubmit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(webui_presubmit)

    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = webui_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/test/data/webui/glic/browser_tests/glic_focus_browsertest.ts',
          ['// Copyright 2026'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = webui_presubmit.CheckChangeOnUpload(
      mock_input_api, mock_output_api
    )
    self.assertEqual(results, [])

  def testWebUiGlicPresubmitIgnoresSupportFile(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    webui_dir = os.path.join(
      repo_root, 'chrome', 'test', 'data', 'webui', 'glic'
    )
    presubmit_path = os.path.join(webui_dir, 'PRESUBMIT.py')
    spec = importlib.util.spec_from_file_location(
      'webui_glic_presubmit', presubmit_path
    )
    webui_presubmit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(webui_presubmit)

    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = webui_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/test/data/webui/glic/browser_tests/browser_test_base.ts',
          ['export function testStepper() {}'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = webui_presubmit.CheckChangeOnUpload(
      mock_input_api, mock_output_api
    )
    self.assertEqual(results, [])

  def testGlicPresubmitIgnoresNonTestCppFile(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/glic_keyed_service.cc',
          ['void Service() {}'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testSharingPresubmitTriggersOnBrowserTest(self):
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    sharing_dir = os.path.join(
      repo_root, 'chrome', 'browser', 'sharing', 'glic_experimental_triggering'
    )
    presubmit_path = os.path.join(sharing_dir, 'PRESUBMIT.py')
    spec = importlib.util.spec_from_file_location(
      'sharing_glic_presubmit', presubmit_path
    )
    sharing_presubmit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sharing_presubmit)

    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = sharing_dir
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/sharing/glic_experimental_triggering/'
          'glic_experimental_triggering_message_handler_browsertest.cc',
          ['// Copyright 2026'],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = sharing_presubmit.CheckChangeOnUpload(
      mock_input_api, mock_output_api
    )
    self.assertEqual(results, [])


class CheckGlicFeaturesDefaultStateTest(unittest.TestCase):
  def setUp(self):
    self._old_sys_path = sys.path[:]
    repo_root = PRESUBMIT_test_mocks._REPO_ROOT
    if repo_root not in sys.path:
      sys.path.insert(0, repo_root)
    from chrome.browser.resources.glic import common_checks

    common_checks.ResetCommonChecksForTesting()

  def tearDown(self):
    sys.path = self._old_sys_path

  def testNoAffectedFiles(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testGlicFeatureEnabledByDefaultInGlicDir(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/public/features.cc',
          [
            'BASE_FEATURE(kGlicNewFeature, base::FEATURE_ENABLED_BY_DEFAULT);',
          ],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertIn('kGlicNewFeature', results[0].items[0])
    self.assertIn(
      'FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS', results[0].message
    )

  def testGlicFeatureWithExplicitEnabledMacros(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/public/features.cc',
          [
            'BASE_FEATURE(kGlicFeature1, '
            'FEATURE_ENABLED_BY_DEFAULT_NON_ANDROID);',
            'BASE_FEATURE(kGlicFeature2, '
            'FEATURE_ENABLED_BY_DEFAULT_ANDROID_ONLY);',
            'BASE_FEATURE(kGlicFeature3, '
            'FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS);',
          ],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testGlicFeatureDisabledByDefaultInGlicDir(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/browser/glic/public/features.cc',
          [
            'BASE_FEATURE(kGlicNewFeature, base::FEATURE_DISABLED_BY_DEFAULT);',
          ],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testGlicFeatureEnabledByDefaultInChromeCommon(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/common/chrome_features.cc',
          [
            'BASE_FEATURE(kGlicFeatureInCommon, '
            'base::FEATURE_ENABLED_BY_DEFAULT);',
          ],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertIn('kGlicFeatureInCommon', results[0].items[0])
    self.assertIn(
      'FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS', results[0].message
    )

  def testNonGlicFeatureEnabledByDefaultInChromeCommon(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.InitFiles(
      [
        PRESUBMIT_test_mocks.MockFile(
          'chrome/common/chrome_features.cc',
          [
            'BASE_FEATURE(kSomeOtherFeature, '
            'base::FEATURE_ENABLED_BY_DEFAULT);',
          ],
        ),
      ]
    )
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])

  def testUnrelatedLineChangedDoesNotTriggerError(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_file = PRESUBMIT_test_mocks.MockFile(
      'chrome/browser/glic/public/features.cc',
      [
        '// Updated header comment',
        'BASE_FEATURE(kGlicExistingFeature, base::FEATURE_ENABLED_BY_DEFAULT);',
      ],
    )
    # Simulate only modifying line 1 (the comment)
    mock_file._changed_contents = [(1, '// Updated header comment')]
    mock_input_api.InitFiles([mock_file])
    mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    results = PRESUBMIT.CheckChangeOnUpload(mock_input_api, mock_output_api)
    self.assertEqual(results, [])


# Include tool unit tests so they execute with PRESUBMIT_test.py
sys.path.insert(
  0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tools')
)
from check_glic_api_test_registration_test import (  # noqa: E402
  CheckGlicApiTestRegistrationCliAndRepoTest,  # noqa: F401
  CheckGlicApiTestRegistrationCppExtractorTest,  # noqa: F401
  CheckGlicApiTestRegistrationCppHandlingTest,  # noqa: F401
  CheckGlicApiTestRegistrationFileTypeTest,  # noqa: F401
  CheckGlicApiTestRegistrationPairMatchingTest,  # noqa: F401
  CheckGlicApiTestRegistrationRepoRootTest,  # noqa: F401
  CheckGlicApiTestRegistrationTsHandlingTest,  # noqa: F401
  CheckGlicApiTestRegistrationTypeScriptExtractorTest,  # noqa: F401
)
from sort_headers_test import SortHeadersTest  # noqa: E402, F401

_glic_presubmit_path = os.path.join(
  os.path.dirname(os.path.abspath(__file__)), 'PRESUBMIT.py'
)
_glic_spec = importlib.util.spec_from_file_location(
  'glic_presubmit_module', _glic_presubmit_path
)
_glic_presubmit = importlib.util.module_from_spec(_glic_spec)
_glic_spec.loader.exec_module(_glic_presubmit)


_metadata_presubmit_path = os.path.join(
  os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
  ),
  'tools',
  'metrics',
  'histograms',
  'metadata',
  'glic',
  'PRESUBMIT.py',
)
_metadata_spec = importlib.util.spec_from_file_location(
  'metadata_glic_presubmit_module', _metadata_presubmit_path
)
_metadata_presubmit = importlib.util.module_from_spec(_metadata_spec)
_metadata_spec.loader.exec_module(_metadata_presubmit)


class CheckGlicMetricsTest(unittest.TestCase):
  """Tests for the Glic metrics checks in both PRESUBMIT.py files."""

  def setUp(self):
    self.mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    self.mock_output_api = PRESUBMIT_test_mocks.MockOutputApi()
    self.histograms_path = (
      'tools/metrics/histograms/metadata/glic/histograms.xml'
    )

    self.old_histograms = """<histogram-configuration><histograms>
<histogram name="Glic.Old" enum="Boolean">
  <summary>Old</summary>
</histogram>
</histograms></histogram-configuration>"""
    self.xml_content = """<histogram-configuration><histograms>
<histogram name="Glic{GlicProvider}.SplitMetric" enum="Boolean"/>
<histogram name="Glic{GlicProvider}.Instance.Metrics.Error{Cause}"
           enum="Boolean"/>
<histogram name="Glic.UnsplitMetric" enum="Boolean"/>
</histograms></histogram-configuration>"""

  def _run_glic_presubmit(self, mock_files, read_file_mock=None):
    """Runs chrome/browser/glic's CheckGlicMetrics on mock_files."""
    self.mock_input_api.files = mock_files
    self.mock_input_api.change = PRESUBMIT_test_mocks.MockChange(
      self.mock_input_api.files
    )
    if read_file_mock:
      self.mock_input_api.ReadFile = read_file_mock
    return _glic_presubmit.CheckGlicMetrics(
      self.mock_input_api, self.mock_output_api
    )

  def _run_metadata_presubmit(self, mock_files, read_file_mock=None):
    """Runs metadata/glic's CheckGlicMetrics on mock_files."""
    self.mock_input_api.files = mock_files
    self.mock_input_api.change = PRESUBMIT_test_mocks.MockChange(
      self.mock_input_api.files
    )
    if read_file_mock:
      self.mock_input_api.ReadFile = read_file_mock
    return _metadata_presubmit.CheckGlicMetrics(
      self.mock_input_api, self.mock_output_api
    )

  def mock_read_file_histograms(self, path, default=None):
    """Returns the old histograms.xml for its path, else default."""
    if path.endswith(self.histograms_path):
      return self.old_histograms
    return ''

  def mock_read_file_xml(self, path, default=None):
    """Returns the old histograms.xml contents for any path."""
    if path.endswith(self.histograms_path):
      return self.xml_content
    return ''

  def test_new_unsplit_without_comment(self):
    """An added unsplit Glic histogram without a marker fails."""
    new_xml = """<histogram-configuration><histograms>
<histogram name="Glic.NewUnsplit" enum="Boolean">
  <summary>Oops no token</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is not split by provider", results[0].message)

  def test_new_unsplit_with_allowed_comment(self):
    """An added unsplit histogram with a reasoned marker passes."""
    new_xml = """<histogram-configuration><histograms>
<histogram name="Glic.NewUnsplit" enum="Boolean">
  <!-- Allowed unsplittable: valid reason -->
  <summary>Token is absent</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 0)

  def test_new_unsplit_with_empty_reason(self):
    """An allowed-unsplittable marker without a reason fails."""
    new_xml = """<histogram-configuration><histograms>
<histogram name="Glic.NewUnsplit" enum="Boolean">
  <!-- Allowed unsplittable: -->
  <summary>Token absent</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 1)
    self.assertTrue(any("missing reason" in r.message for r in results))

  def test_bad_dot(self):
    """A provider token followed by a dot is rejected."""
    new_xml = """<histogram-configuration><histograms>
<variants name="GlicProvider">
  <variant name=".Consumer" summary="C"/>
  <variant name=".Enterprise" summary="E"/>
</variants>
<histogram name="Glic.{GlicProvider}.BadDot" enum="Boolean">
  <token key="GlicProvider" variants="GlicProvider"/>
  <summary>Bad dot before provider token.</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 2)
    self.assertIn("is not split by provider", results[0].message)

  def test_split_added_histogram_passes(self):
    """An added Glic{GlicProvider} histogram passes."""
    new_xml = """<histogram-configuration><histograms>
<variants name="GlicProvider">
  <variant name=".Consumer" summary="C"/>
  <variant name=".Enterprise" summary="E"/>
</variants>
<histogram name="Glic{GlicProvider}.New" enum="Boolean">
  <token key="GlicProvider" variants="GlicProvider"/>
  <summary>Scope: {GlicProvider}.</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 0)

  def test_pre_existing_unsplit_stays_clean(self):
    """Unsplit histograms already in old contents are not flagged."""
    new_xml = """<histogram-configuration><histograms>
<histogram name="Glic.Old" enum="Boolean">
  <summary>Old with edits</summary>
</histogram>
</histograms></histogram-configuration>"""
    results = self._run_metadata_presubmit(
      [
        PRESUBMIT_test_mocks.MockAffectedFile(
          self.histograms_path, [new_xml], [self.old_histograms]
        )
      ],
      self.mock_read_file_histograms,
    )
    self.assertEqual(len(results), 0)

  def testFlagsRawCallToSplitNameEvenWithComment(self):
    """Verifies split metric raw call is flagged even with comment."""
    file_content = [
      "// Justification: reason",
      "base::UmaHistogramBoolean(\"Glic.SplitMetric\", true);",
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

  def testFlagsSplitNameWithTokenMatchVariant(self):
    """Verifies split metric with variant token match is flagged."""
    file_content = [
      "UMA_HISTOGRAM_ENUMERATION(\"Glic.Instance.Metrics.Error.ByUser\", val);"
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

  def testFlagsUnsplitRawCallWithoutComment(self):
    """Verifies unsplit metric raw call without justification is flagged."""
    file_content = ["base::UmaHistogramBoolean(\"Glic.UnsplitMetric\", true);"]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)
    self.assertIn(
      "requires a '// Justification: <reason>' comment", results[0].message
    )

  def testCleanUnsplitRawCallWithComment(self):
    """Verifies unsplit metric raw call with justification passes."""
    file_content = [
      "// Justification: legacy metric",
      "base::UmaHistogramBoolean(\"Glic.UnsplitMetric\", true);",
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testFlagsUnsplitRawCallWithBadComment(self):
    """Verifies unsplit metric raw call with bad justification is flagged."""
    file_content = [
      "// some comment",
      "base::UmaHistogramBoolean(\"Glic.UnsplitMetric\", true);",
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)

  def testCleanUnsplitRawCallWithMultiLineComment(self):
    """Verifies unsplit metric raw call with multi-line comment passes."""
    file_content = [
      "// Justification: line 1",
      "// line 2",
      "base::UmaHistogramBoolean(\"Glic.UnsplitMetric\", true);",
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testCleanGlicMetricReporter(self):
    """Verifies recording via GlicMetricReporter passes."""
    file_content = ["metric_reporter.RecordBoolean(\"SplitMetric\", true);"]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testCleanTestFiles(self):
    """Verifies test files are exempt from raw call checks."""
    file_content = ["base::UmaHistogramBoolean(\"Glic.SplitMetric\", true);"]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file_unittest.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testCleanUserMetricsAction(self):
    """Verifies base::UserMetricsAction calls are exempt."""
    file_content = [
      "base::RecordAction(base::UserMetricsAction(\"Glic.SplitMetric\"));"
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testFlagsMultilineWrappedCall(self):
    """Verifies multiline wrapped raw UMA call is flagged."""
    file_content = [
      "// Justification: reason",
      "base::UmaHistogramEnumeration(",
      "    \"Glic.SplitMetric\", val);",
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

  def testCleanCommentedOutCall(self):
    """Verifies commented-out UMA call is ignored."""
    file_content = ["// base::UmaHistogramBoolean(\"Glic.SplitMetric\", true);"]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 0)

  def testSplitMetricOnUnchangedLineInAffectedFile(self):
    """Verifies split metric on unchanged line in affected file is flagged."""
    file_content = [
      "base::UmaHistogramBoolean(\"Glic.SplitMetric\", true);",
      "// changed line",
    ]
    mock_file = PRESUBMIT_test_mocks.MockAffectedFile(
      'chrome/browser/glic/some_file.cc',
      file_content,
      ["base::UmaHistogramBoolean(\"Glic.SplitMetric\", true);"],
      action='M',
    )
    mock_file.ChangedContents = lambda: [(2, "// changed line")]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [mock_xml, mock_file], self.mock_read_file_xml
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

  def testWrappedCallSecondLineModified(self):
    """Verifies wrapped UmaHistogram edits 2nd line skips Justif."""
    file_content = [
      "base::UmaHistogramBoolean(",
      "    \"Glic.UnsplitMetric\", x);",
    ]
    old_content = [
      "base::UmaHistogramBoolean(",
      "    \"Glic.UnsplitMetric\", y);",
    ]
    mock_file = PRESUBMIT_test_mocks.MockAffectedFile(
      'chrome/browser/glic/some_file.cc', file_content, old_content, action='M'
    )
    mock_file.ChangedContents = lambda: [(2, file_content[1])]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [mock_xml, mock_file], self.mock_read_file_xml
    )
    self.assertEqual(len(results), 1)
    self.assertIn(
      "requires a '// Justification: <reason>' comment immediately above",
      results[0].message,
    )

  def testOutsideGlicDirIgnored(self):
    """Verifies that metric in file outside glic dir produces NO error."""
    file_content = [
      "base::UmaHistogramBoolean(",
      "    \"Glic.SplitMetric\", x);",
    ]
    mock_file = PRESUBMIT_test_mocks.MockAffectedFile(
      'chrome/browser/ui/foo.cc', file_content, file_content, action='M'
    )
    mock_file.ChangedContents = lambda: [(2, file_content[1])]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [mock_xml, mock_file], self.mock_read_file_xml
    )
    self.assertEqual(len(results), 0)

  def testWrappedCallSplitMetricFullScan(self):
    """Verifies split metric in full scan causes exactly 1 error."""
    file_content = [
      "base::UmaHistogramBoolean(",
      "    \"Glic.SplitMetric\", x);",
    ]
    mock_file = PRESUBMIT_test_mocks.MockAffectedFile(
      'chrome/browser/glic/some_file.cc', file_content, file_content, action='M'
    )
    mock_file.ChangedContents = lambda: []
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [mock_xml, mock_file], self.mock_read_file_xml
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

  def testXmlChangeTriggersCppScan(self):
    """Verifies XML change triggers C++ scan."""
    new_xml = """<histogram-configuration><histograms>
<variants name="GlicProvider">
  <variant name=".Consumer" summary="C"/>
  <variant name=".Enterprise" summary="E"/>
</variants>
<histogram name="Glic{GlicProvider}.Foo" enum="Boolean">
  <token key="GlicProvider" variants="GlicProvider"/>
  <summary>Scope: {GlicProvider}.</summary>
</histogram>
</histograms></histogram-configuration>"""
    file_content = ["base::UmaHistogramBoolean(\"Glic.Foo\", true);"]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [new_xml], [self.old_histograms]
    )
    mock_cpp = PRESUBMIT_test_mocks.MockAffectedFile(
      'chrome/browser/glic/some_file.cc', file_content, file_content, action='M'
    )
    mock_cpp.ChangedContents = lambda: []

    results = self._run_glic_presubmit(
      [mock_xml, mock_cpp], self.mock_read_file_histograms
    )
    self.assertEqual(len(results), 1)
    self.assertIn("is split by provider; record it via", results[0].message)

    # The metadata presubmit leaves the C++ scan to chrome/browser/glic's
    # PRESUBMIT.py when both run, so the error is not reported twice.
    results = self._run_metadata_presubmit(
      [mock_xml, mock_cpp], self.mock_read_file_histograms
    )
    self.assertEqual(len(results), 0)

  def testNewlyCreatedHistogramsXmlDoesNotReadSelfAsOld(self):
    """Verifies newly created XML does not read itself as old contents."""
    new_xml = """<histogram-configuration><histograms>
<histogram name="Glic.NewUnsplit" enum="Boolean">
  <!-- Allowed unsplittable: reason -->
  <summary>Token is absent</summary>
</histogram>
</histograms></histogram-configuration>"""
    mock_file = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [new_xml], []
    )
    mock_file.OldContents = lambda: []
    results = self._run_metadata_presubmit(
      [mock_file], self.mock_read_file_histograms
    )
    self.assertEqual(len(results), 0)

  def testFlagsSplitNameWithTokenMatchEmptyVariant(self):
    """Verifies split metric with empty variant token match is flagged."""
    file_content = [
      "UMA_HISTOGRAM_ENUMERATION(\"Glic.Instance.Metrics.Error\", val);"
    ]
    mock_xml = PRESUBMIT_test_mocks.MockAffectedFile(
      self.histograms_path, [self.xml_content], []
    )
    results = self._run_glic_presubmit(
      [
        mock_xml,
        PRESUBMIT_test_mocks.MockAffectedFile(
          'chrome/browser/glic/some_file.cc',
          file_content,
          file_content,
          action='A',
        ),
      ],
      self.mock_read_file_xml,
    )
    self.assertEqual(len(results), 1)


if __name__ == '__main__':
  unittest.main()

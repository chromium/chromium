#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from os_settings_presubmit_checker import OSSettingsPresubmitChecker

# Update system path to src/ so we can access src/PRESUBMIT_test_mocks.py.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..',
                 '..', '..'))

from PRESUBMIT_test_mocks import (MockInputApi, MockOutputApi,
                                  MockAffectedFile)


class OSSettingsPresubmitCheckerTest(unittest.TestCase):
    def testAddSingletonGetterUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_browser_proxy.js', [
                "import {addSingletonGetter} from 'chrome://resources/js/cr.js';",
                '',
                'addSingletonGetter(ExampleBrowserProxyImpl);',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(2, len(errors))

    def testAddSingletonGetterNotUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_browser_proxy.js', [
                'static getInstance() {',
                '  return instance || (instance = new ExampleBrowserProxyImpl());',
                '}',
                'static setInstanceForTesting(obj) {',
                '  instance = obj;',
                '}',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))

    def testLegacyPolymerSyntaxUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_element.js', [
                'Polymer({',
                "  is: 'example-element',",
                '});',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(1, len(errors))

    def testLegacyPolymerSyntaxNotUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_element.js', [
                'class ExampleElement extends PolymerElement {',
                '  static get is() {',
                "    return 'example-element';",
                "  }",
                '}',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))

    def testSchemeSpecificURLsUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_element.js', [
                'import \'chrome://resources/utils.js\';',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))

    def testSchemeSpecificURLsNotUsed(self):
        mock_input_api = MockInputApi()
        mock_input_api.files = [
            MockAffectedFile('chrome/example_element.js', [
                'import \'chrome://resources/good.js\';',
                'import \'//resources/bad.js\';',
                'import \'//resources/bad_again.js\';',
            ]),
        ]
        mock_output_api = MockOutputApi()

        errors = OSSettingsPresubmitChecker.RunChecks(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(2, len(errors))


if __name__ == '__main__':
    unittest.main()

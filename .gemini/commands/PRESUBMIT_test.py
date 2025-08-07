#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

_DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir))
sys.path.append(_DIR_SOURCE_ROOT)

from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockAffectedFile


class CheckCommandsTest(unittest.TestCase):
    def test_require_cr_scope(self):
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            MockAffectedFile('.gemini/commands/command.toml', [
                'description = "fake description"',
                'prompt = "fake prompt"',
            ]),
        ])

        results = PRESUBMIT.CheckCommands(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertRegex(
            results[0].message,
            r'Move \.gemini[/\\]commands[/\\]command.toml under '
            r'[^\s]*\.gemini[/\\]commands[/\\]cr')

    def test_require_valid_toml(self):
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            MockAffectedFile('.gemini/commands/cr/command.toml', [
                'invalid',
            ]),
        ])

        results = PRESUBMIT.CheckCommands(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertRegex(
            results[0].message,
            r'\.gemini[/\\]commands[/\\]cr[/\\]command.toml is not valid TOML')

    def test_require_prompt(self):
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            MockAffectedFile('.gemini/commands/cr/command.toml', [
                'description = "fake description"',
            ]),
        ])

        results = PRESUBMIT.CheckCommands(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('error', results[0].type)
        self.assertRegex(
            results[0].message,
            r'\.gemini[/\\]commands[/\\]cr[/\\]command.toml must define '
            'a `prompt` key')

    def test_recommend_definition(self):
        mock_input_api = MockInputApi()
        mock_input_api.InitFiles([
            MockAffectedFile('.gemini/commands/cr/command.toml', [
                'prompt = "fake prompt"',
            ]),
        ])

        results = PRESUBMIT.CheckCommands(mock_input_api, MockOutputApi())
        self.assertEqual(1, len(results))
        self.assertEqual('warning', results[0].type)
        self.assertRegex(
            results[0].message,
            r'\.gemini[/\\]commands[/\\]cr[/\\]command.toml should define '
            'a `description` key')


if __name__ == '__main__':
    unittest.main()

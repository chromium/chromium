#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gemini_cli_unittests."""

import pathlib
import subprocess
import unittest
import unittest.mock

import gemini_cli_installation


class GeminiCliInstallationTest(unittest.TestCase):

    @unittest.mock.patch('subprocess.check_call')
    @unittest.mock.patch('gemini_cli_installation.CIPD_ROOT',
                         pathlib.Path('/fake/cipd/root'))
    def test_fetch_cipd_gemini_cli_not_verbose(self, mock_check_call):
        gemini_path, node_path = gemini_cli_installation.fetch_cipd_gemini_cli(
            verbose=False)

        self.assertEqual(
            gemini_path,
            pathlib.Path('/fake/cipd/root/node_modules/.bin/gemini'))
        self.assertEqual(node_path, pathlib.Path('/fake/cipd/root/bin/node'))

        expected_calls = [
            unittest.mock.call([
                'cipd', 'init', '-force',
                str(pathlib.Path('/fake/cipd/root'))
            ]),
            unittest.mock.call(
                [
                    'cipd',
                    'install',
                    'infra/3pp/tools/nodejs/linux-${arch}',
                    'version:3@25.0.0',
                    '-root',
                    pathlib.Path('/fake/cipd/root'),
                    '-log-level',
                    'warning',
                ],
                stdout=subprocess.DEVNULL,
            ),
            unittest.mock.call(
                [
                    'cipd',
                    'install',
                    'infra/3pp/npm/gemini-cli/linux-${arch}',
                    'version:3@0.9.0',
                    '-root',
                    pathlib.Path('/fake/cipd/root'),
                    '-log-level',
                    'warning',
                ],
                stdout=subprocess.DEVNULL,
            ),
        ]
        mock_check_call.assert_has_calls(expected_calls)
        self.assertEqual(mock_check_call.call_count, len(expected_calls))

    @unittest.mock.patch('subprocess.check_call')
    @unittest.mock.patch('gemini_cli_installation.CIPD_ROOT',
                         pathlib.Path('/fake/cipd/root'))
    def test_fetch_cipd_gemini_cli_verbose(self, mock_check_call):
        gemini_cli_installation.fetch_cipd_gemini_cli(verbose=True)

        expected_calls = [
            unittest.mock.call([
                'cipd', 'init', '-force',
                str(pathlib.Path('/fake/cipd/root'))
            ]),
            unittest.mock.call(
                [
                    'cipd',
                    'install',
                    'infra/3pp/tools/nodejs/linux-${arch}',
                    'version:3@25.0.0',
                    '-root',
                    pathlib.Path('/fake/cipd/root'),
                    '-log-level',
                    'debug',
                ],
                stdout=subprocess.DEVNULL,
            ),
            unittest.mock.call(
                [
                    'cipd',
                    'install',
                    'infra/3pp/npm/gemini-cli/linux-${arch}',
                    'version:3@0.9.0',
                    '-root',
                    pathlib.Path('/fake/cipd/root'),
                    '-log-level',
                    'debug',
                ],
                stdout=subprocess.DEVNULL,
            ),
        ]
        mock_check_call.assert_has_calls(expected_calls)
        self.assertEqual(mock_check_call.call_count, len(expected_calls))


if __name__ == '__main__':
    unittest.main()

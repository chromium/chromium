#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for git_cl_helper.py."""

import json
import subprocess
import unittest
from unittest.mock import MagicMock, patch

import git_cl_helper


class GitClHelperTest(unittest.TestCase):
    # pylint: disable=protected-access

    @patch('git_cl_helper.subprocess.run')
    def test_fetch_build_error_success(self, mock_run):
        mock_run.return_value = subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout='{"summaryMarkdown": "Compilation failed"}',
            stderr='')

        summary = git_cl_helper._fetch_build_error("Failed build: build/12345")
        self.assertEqual(summary, 'Compilation failed')

    @patch('git_cl_helper.subprocess.run')
    def test_fetch_build_error_failure(self, mock_run):
        mock_run.side_effect = Exception("bb failed")

        summary = git_cl_helper._fetch_build_error("Failed build: build/12345")
        self.assertEqual(summary, 'Failed build: build/12345'[:3000])

    @patch('git_cl_helper.urllib.request.urlopen')
    def test_poll_gerrit_passed(self, mock_urlopen):
        mock_resp = MagicMock()
        # Gerrit API prefix )]}'
        mock_resp.read.return_value = b")]}'\n" + json.dumps({
            "messages": [{
                "author": {
                    "email": ("chromium-scoped@luci-project-accounts."
                              "iam.gserviceaccount.com")
                },
                "message": "Dry run: This CL passed the run."
            }]
        }).encode('utf-8')
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        status, detail = git_cl_helper._poll_gerrit(
            'https://chromium-review.googlesource.com/c/chromium/src/+/123456')
        self.assertEqual(status, 'passed')
        self.assertEqual(detail, '')

    @patch('git_cl_helper.urllib.request.urlopen')
    def test_poll_gerrit_failed(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.read.return_value = b")]}'\n" + json.dumps({
            "messages": [{
                "author": {
                    "email": ("chromium-scoped@luci-project-accounts."
                              "iam.gserviceaccount.com")
                },
                "message":
                "Dry run: This CL has failed.\nFailed builds: build/12345"
            }]
        }).encode('utf-8')
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        # Mock _fetch_build_error to avoid calling bb
        with patch('git_cl_helper._fetch_build_error') as mock_fetch:
            mock_fetch.return_value = "Summary of failure"
            status, detail = git_cl_helper._poll_gerrit(
                'https://chromium-review.googlesource.com/'
                'c/chromium/src/+/123456')

        self.assertEqual(status, 'failed')
        self.assertEqual(detail, 'Summary of failure')

    @patch('git_cl_helper.urllib.request.urlopen')
    def test_poll_gerrit_running(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.read.return_value = b")]}'\n" + json.dumps({
            "messages": [{
                "author": {
                    "email": ("chromium-scoped@luci-project-accounts."
                              "iam.gserviceaccount.com")
                },
                "message": "Dry run: CV is trying the patch."
            }]
        }).encode('utf-8')
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        status, detail = git_cl_helper._poll_gerrit(
            'https://chromium-review.googlesource.com/c/chromium/src/+/123456')
        self.assertEqual(status, 'running')
        self.assertEqual(detail, '')

    @patch('git_cl_helper.urllib.request.urlopen')
    def test_poll_gerrit_rebase_conflict(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.read.return_value = b")]}'\n" + json.dumps({
            "messages": [{
                "author": {
                    "email": ("chromium-scoped@luci-project-accounts."
                              "iam.gserviceaccount.com")
                },
                "message": "Patch failure: Try rebasing."
            }]
        }).encode('utf-8')
        mock_urlopen.return_value.__enter__.return_value = mock_resp

        status, detail = git_cl_helper._poll_gerrit(
            'https://chromium-review.googlesource.com/c/chromium/src/+/123456')
        self.assertEqual(status, 'rebase_conflict')
        self.assertTrue('Patch failure' in detail)


if __name__ == '__main__':
    unittest.main()

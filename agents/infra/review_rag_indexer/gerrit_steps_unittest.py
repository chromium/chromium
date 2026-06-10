#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for gerrit_steps.py."""

import datetime
import json
import unittest
from unittest import mock

import requests

from common_types import ClInfo, CommonArgs
import gerrit_steps

# pylint: disable=protected-access


class FetchHashtagsForClTest(unittest.TestCase):

    def setUp(self):
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(2026,
                                          6,
                                          2,
                                          11,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_session_class = mock.patch('requests.Session').start()
        self.mock_get = self.mock_session_class.return_value.get
        self.mock_sleep = mock.patch('time.sleep').start()
        self.addCleanup(mock.patch.stopall)

        self.manager = gerrit_steps._SessionManager()
        self.manager.register_session_for_current_thread()

    def test_session_configuration(self):
        mock_session = self.mock_session_class.return_value
        mock_session.mount.assert_called_once()
        args, _ = mock_session.mount.call_args
        self.assertEqual(args[0], 'https://')
        adapter = args[1]
        self.assertIsInstance(adapter, requests.adapters.HTTPAdapter)
        retry = adapter.max_retries
        self.assertEqual(retry.total, 2)
        self.assertEqual(retry.backoff_factor, 1.0)
        self.assertEqual(retry.status_forcelist, {500, 502, 503, 504})

    def test_success(self):
        mock_response = mock.Mock()
        mock_response.text = ')]}\'\n["tag1", "tag2"]'
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        result = gerrit_steps._fetch_hashtags_for_cl('chromium', self.manager,
                                                     self.cl_info)

        self.assertTrue(result)
        self.assertEqual(self.cl_info.hashtags, {'tag1', 'tag2'})
        self.mock_get.assert_called_once_with(
            'https://chromium-review.googlesource.com/changes/1234/hashtags',
            timeout=30,
        )

    def test_success_no_prefix(self):
        mock_response = mock.Mock()
        mock_response.text = '["tag1", "tag2"]'
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        result = gerrit_steps._fetch_hashtags_for_cl('chromium', self.manager,
                                                     self.cl_info)

        self.assertTrue(result)
        self.assertEqual(self.cl_info.hashtags, {'tag1', 'tag2'})

    def test_failure_returns_false(self):
        self.mock_get.side_effect = requests.exceptions.ConnectionError(
            'Connection aborted')

        with self.assertLogs(level='WARNING') as log:
            result = gerrit_steps._fetch_hashtags_for_cl(
                'chromium', self.manager, self.cl_info)

        self.assertFalse(result)
        self.assertEqual(self.cl_info.hashtags, set())
        self.mock_get.assert_called_once()
        self.assertTrue(
            any('Failed to fetch hashtags' in line for line in log.output))

    def test_bad_json_propagates(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n{invalid json}"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(json.JSONDecodeError):
            gerrit_steps._fetch_hashtags_for_cl('chromium', self.manager,
                                                self.cl_info)

        self.assertEqual(self.mock_get.call_count, 1)
        self.mock_sleep.assert_not_called()

    def test_not_a_list_raises(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n{\"hashtags\": [\"tag1\"]}"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(ValueError) as cm:
            gerrit_steps._fetch_hashtags_for_cl('chromium', self.manager,
                                                self.cl_info)

        self.assertIn('Expected list of hashtags', str(cm.exception))
        self.assertEqual(self.mock_get.call_count, 1)
        self.mock_sleep.assert_not_called()

    def test_success_merges_hashtags(self):
        self.cl_info.hashtags = {'ipc_review', 'existing_tag'}
        mock_response = mock.Mock()
        mock_response.text = ')]}\'\n["tag1", "ipc_review", "tag2"]'
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        result = gerrit_steps._fetch_hashtags_for_cl('chromium', self.manager,
                                                     self.cl_info)

        self.assertTrue(result)
        self.assertEqual(self.cl_info.hashtags,
                         {'ipc_review', 'existing_tag', 'tag1', 'tag2'})

    def test_failure_preserves_hashtags(self):
        self.cl_info.hashtags = {'ipc_review'}
        self.mock_get.side_effect = requests.exceptions.ConnectionError(
            'Connection aborted')

        with self.assertLogs(level='WARNING'):
            result = gerrit_steps._fetch_hashtags_for_cl(
                'chromium', self.manager, self.cl_info)

        self.assertFalse(result)
        self.assertEqual(self.cl_info.hashtags, {'ipc_review'})


class RetrieveHashtagsTest(unittest.TestCase):

    def setUp(self):
        self.common_args = CommonArgs(
            project='chromium',
            repo='chromium/src',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None,
        )
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(2026,
                                          6,
                                          2,
                                          11,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_fetch = mock.patch(
            'gerrit_steps._fetch_hashtags_for_cl').start()
        self.addCleanup(mock.patch.stopall)

    def test_empty(self):
        gerrit_steps.retrieve_hashtags(self.common_args, [])
        self.mock_fetch.assert_not_called()

    def test_multiple(self):
        cl_infos = [
            self.cl_info,
            ClInfo(
                revision='beefdead',
                cl_number=5678,
                commit_time=datetime.datetime(2026,
                                              6,
                                              2,
                                              11,
                                              0,
                                              0,
                                              tzinfo=datetime.timezone.utc),
                commit_position=101,
                description='Test CL 2',
                dir_metadata=mock.Mock(),
            )
        ]
        self.mock_fetch.return_value = True

        gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

        self.assertEqual(self.mock_fetch.call_count, 2)
        self.mock_fetch.assert_has_calls([
            mock.call('chromium', mock.ANY, cl_infos[0]),
            mock.call('chromium', mock.ANY, cl_infos[1]),
        ],
                                         any_order=True)

    def test_under_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(2026,
                                              6,
                                              2,
                                              11,
                                              0,
                                              0,
                                              tzinfo=datetime.timezone.utc),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            ) for i in range(100)
        ]

        self.mock_fetch.side_effect = [False] + [True] * 99

        gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

    def test_over_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(2026,
                                              6,
                                              2,
                                              11,
                                              0,
                                              0,
                                              tzinfo=datetime.timezone.utc),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            ) for i in range(100)
        ]

        self.mock_fetch.side_effect = [False, False] + [True] * 98

        with self.assertRaises(RuntimeError) as cm:
            gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

        self.assertIn('exceeded threshold', str(cm.exception))


if __name__ == '__main__':
    unittest.main()

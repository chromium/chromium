#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for gerrit_steps.py."""

import datetime
import json
import textwrap
import unittest
from unittest import mock

import requests

from common_types import ClInfo, CommentThread, CommonArgs
import gerrit_steps

# pylint: disable=protected-access


class FetchHashtagsForClTest(unittest.TestCase):
    def setUp(self):
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(
                2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
            ),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_session_class = mock.patch('requests.Session').start()
        self.mock_get = self.mock_session_class.return_value.get
        self.mock_sleep = mock.patch('time.sleep').start()

        self.mock_authenticator = mock.Mock()
        mock.patch(
            'gerrit_util._Authenticator.get',
            return_value=self.mock_authenticator,
        ).start()

        self.addCleanup(mock.patch.stopall)

        self.manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )
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

        result = gerrit_steps._fetch_hashtags_for_cl(self.manager, self.cl_info)

        self.assertTrue(result)
        self.assertEqual(self.cl_info.hashtags, {'tag1', 'tag2'})
        self.mock_get.assert_called_once_with(
            'https://chromium-review.googlesource.com/a/changes/1234/hashtags',
            timeout=30,
        )

    def test_success_no_prefix(self):
        mock_response = mock.Mock()
        mock_response.text = '["tag1", "tag2"]'
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        result = gerrit_steps._fetch_hashtags_for_cl(self.manager, self.cl_info)

        self.assertTrue(result)
        self.assertEqual(self.cl_info.hashtags, {'tag1', 'tag2'})

    def test_failure_returns_false(self):
        self.mock_get.side_effect = requests.exceptions.ConnectionError(
            'Connection aborted'
        )

        with self.assertLogs(level='WARNING') as log:
            result = gerrit_steps._fetch_hashtags_for_cl(
                self.manager, self.cl_info
            )

        self.assertFalse(result)
        self.assertEqual(self.cl_info.hashtags, set())
        self.mock_get.assert_called_once()
        self.assertTrue(
            any('Failed to fetch hashtags' in line for line in log.output)
        )

    def test_bad_json_propagates(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n{invalid json}"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(json.JSONDecodeError):
            gerrit_steps._fetch_hashtags_for_cl(self.manager, self.cl_info)

        self.assertEqual(self.mock_get.call_count, 1)
        self.mock_sleep.assert_not_called()

    def test_not_a_list_raises(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n{\"hashtags\": [\"tag1\"]}"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(ValueError) as cm:
            gerrit_steps._fetch_hashtags_for_cl(self.manager, self.cl_info)

        self.assertIn('Expected list of hashtags', str(cm.exception))
        self.assertEqual(self.mock_get.call_count, 1)
        self.mock_sleep.assert_not_called()

    def test_success_merges_hashtags(self):
        self.cl_info.hashtags = {'ipc_review', 'existing_tag'}
        mock_response = mock.Mock()
        mock_response.text = ')]}\'\n["tag1", "ipc_review", "tag2"]'
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        result = gerrit_steps._fetch_hashtags_for_cl(self.manager, self.cl_info)

        self.assertTrue(result)
        self.assertEqual(
            self.cl_info.hashtags,
            {'ipc_review', 'existing_tag', 'tag1', 'tag2'},
        )

    def test_failure_preserves_hashtags(self):
        self.cl_info.hashtags = {'ipc_review'}
        self.mock_get.side_effect = requests.exceptions.ConnectionError(
            'Connection aborted'
        )

        with self.assertLogs(level='WARNING'):
            result = gerrit_steps._fetch_hashtags_for_cl(
                self.manager, self.cl_info
            )

        self.assertFalse(result)
        self.assertEqual(self.cl_info.hashtags, {'ipc_review'})

    def test_session_auth_cookies(self):
        self.mock_authenticator.authenticate.side_effect = lambda conn: (
            conn.req_headers.update({'Authorization': 'Bearer token'})
        )

        manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )

        mock_session = mock.Mock()
        mock_session.headers = {}
        self.mock_session_class.return_value = mock_session

        manager.register_session_for_current_thread()

        self.assertEqual(
            mock_session.headers, {'Authorization': 'Bearer token'}
        )
        self.assertEqual(
            mock_session.gerrit_base_url,
            'https://chromium-review.googlesource.com/a',
        )

    def test_session_auth_sso(self):

        def sso_auth(conn):
            conn.req_headers.update({'Cookie': 'sso_cookie'})
            conn.req_uri = 'http://chromium.git.corp.google.com/a/'

            class MockProxy:
                proxy_host = b'localhost'
                proxy_port = 8080

            conn.proxy_info = MockProxy()

        self.mock_authenticator.authenticate.side_effect = sso_auth

        manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )

        mock_session = mock.Mock()
        mock_session.headers = {}
        self.mock_session_class.return_value = mock_session

        manager.register_session_for_current_thread()

        self.assertEqual(mock_session.headers, {'Cookie': 'sso_cookie'})
        self.assertEqual(
            mock_session.proxies,
            {
                'http': 'http://localhost:8080',
                'https': 'http://localhost:8080',
            },
        )
        self.assertEqual(
            mock_session.gerrit_base_url,
            'http://chromium.git.corp.google.com/a',
        )

    def test_session_auth_failure(self):
        self.mock_authenticator.authenticate.side_effect = Exception(
            'Auth error'
        )

        manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )

        with self.assertRaises(RuntimeError) as cm:
            manager.register_session_for_current_thread()

        self.assertIn(
            'Failed to authenticate for chromium-review.googlesource.com',
            str(cm.exception),
        )


class RetrieveHashtagsTest(unittest.TestCase):
    def setUp(self):
        self.common_args = CommonArgs(
            project='chromium',
            repo='chromium/src',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(
                2026, 6, 2, 12, 0, 0, tzinfo=datetime.timezone.utc
            ),
            dryrun=False,
            previous_run=None,
        )
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(
                2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
            ),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_fetch = mock.patch(
            'gerrit_steps._fetch_hashtags_for_cl'
        ).start()
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
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=101,
                description='Test CL 2',
                dir_metadata=mock.Mock(),
            ),
        ]
        self.mock_fetch.return_value = True

        gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

        self.assertEqual(self.mock_fetch.call_count, 2)
        self.mock_fetch.assert_has_calls(
            [
                mock.call(mock.ANY, cl_infos[0]),
                mock.call(mock.ANY, cl_infos[1]),
            ],
            any_order=True,
        )

    def test_under_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            )
            for i in range(100)
        ]

        self.mock_fetch.side_effect = [False] + [True] * 99

        gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

    def test_over_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            )
            for i in range(100)
        ]

        self.mock_fetch.side_effect = [False, False] + [True] * 98

        with self.assertRaises(RuntimeError) as cm:
            gerrit_steps.retrieve_hashtags(self.common_args, cl_infos)

        self.assertIn('exceeded threshold', str(cm.exception))


class GerritAuthIntegrationTest(unittest.TestCase):
    """Integration tests that use real gerrit_util authenticators.

    These tests assume the environment has some valid way to authenticate
    (e.g., .gitcookies or SSO) and verify that the integration with
    depot_tools works without raising exceptions.
    """

    def test_real_auth_integration(self):
        manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )
        session = requests.Session()

        manager._configure_session_auth(session)

        self.assertTrue(hasattr(session, 'gerrit_base_url'))
        self.assertTrue(
            session.gerrit_base_url.startswith(('http://', 'https://'))
        )
        self.assertTrue(session.gerrit_base_url.endswith('/a'))


class ReconstructThreadsForFileTest(unittest.TestCase):
    def test_empty(self):
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', [])
        self.assertEqual(threads, [])

    def test_single_comment_omitted(self):
        comments = [
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Nit: whitespace',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            }
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(threads, [])

    def test_multiple_independent_comments_omitted(self):
        comments = [
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Nit: whitespace',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 1,
                'message': 'Nit: naming',
                'updated': '2013-02-26 15:40:44.000000000',
                'author': {'name': 'Jane Roe'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(threads, [])

    def test_linear_thread(self):
        comments = [
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Nit: whitespace',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 2,
                'in_reply_to': 'c1',
                'message': 'Done',
                'updated': '2013-02-26 15:40:45.000000000',
                'author': {'name': 'Jane Roe'},
            },
            {
                'id': 'c3',
                'patch_set': 2,
                'in_reply_to': 'c2',
                'message': 'Thanks',
                'updated': '2013-02-26 15:40:46.000000000',
                'author': {'name': 'John Doe'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(len(threads), 1)
        self.assertEqual(threads[0].patch_set, 1)  # Root patchset
        expected_markdown = textwrap.dedent("""\
            # Comment 1 (John Doe)

            Nit: whitespace

            # Comment 2 (Jane Roe)

            Done

            # Comment 3 (John Doe)

            Thanks""")
        self.assertEqual(threads[0].thread_markdown, expected_markdown)

    def test_branching_thread(self):
        comments = [
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Nit: whitespace',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 2,
                'in_reply_to': 'c1',
                'message': 'Done',
                'updated': '2013-02-26 15:40:45.000000000',
                'author': {'name': 'Jane Roe'},
            },
            {
                'id': 'c3',
                'patch_set': 2,
                'in_reply_to': 'c1',
                'message': 'I disagree',
                'updated': '2013-02-26 15:40:46.000000000',
                'author': {'name': 'Bob Smith'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(len(threads), 1)
        # DFS order, sorted by updated time for siblings.
        # c1 -> c2 (updated 45) -> c3 (updated 46)
        # So DFS should visit c1, then c2, then c3.
        expected_markdown = textwrap.dedent("""\
            # Comment 1 (John Doe)

            Nit: whitespace

            # Comment 2 (Jane Roe)

            Done

            # Comment 3 (Bob Smith)

            I disagree""")
        self.assertEqual(threads[0].thread_markdown, expected_markdown)

    def test_multiple_threads(self):
        comments = [
            # Thread 1
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Thread 1 root',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 1,
                'in_reply_to': 'c1',
                'message': 'Thread 1 reply',
                'updated': '2013-02-26 15:40:45.000000000',
                'author': {'name': 'Jane Roe'},
            },
            # Thread 2
            {
                'id': 'c3',
                'patch_set': 2,
                'message': 'Thread 2 root',
                'updated': '2013-02-26 15:41:43.000000000',
                'author': {'name': 'Bob Smith'},
            },
            {
                'id': 'c4',
                'patch_set': 2,
                'in_reply_to': 'c3',
                'message': 'Thread 2 reply',
                'updated': '2013-02-26 15:41:45.000000000',
                'author': {'name': 'Alice Jones'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(len(threads), 2)

        self.assertEqual(threads[0].patch_set, 1)
        expected_markdown_1 = textwrap.dedent("""\
            # Comment 1 (John Doe)

            Thread 1 root

            # Comment 2 (Jane Roe)

            Thread 1 reply""")
        self.assertEqual(threads[0].thread_markdown, expected_markdown_1)

        self.assertEqual(threads[1].patch_set, 2)
        expected_markdown_2 = textwrap.dedent("""\
            # Comment 1 (Bob Smith)

            Thread 2 root

            # Comment 2 (Alice Jones)

            Thread 2 reply""")
        self.assertEqual(threads[1].thread_markdown, expected_markdown_2)

    def test_multiple_threads_mixed_omission(self):
        comments = [
            # Thread 1 (should be kept)
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Thread 1 root',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 1,
                'in_reply_to': 'c1',
                'message': 'Thread 1 reply',
                'updated': '2013-02-26 15:40:45.000000000',
                'author': {'name': 'Jane Roe'},
            },
            # Thread 2 (should be omitted)
            {
                'id': 'c3',
                'patch_set': 2,
                'message': 'Thread 2 root (single)',
                'updated': '2013-02-26 15:41:43.000000000',
                'author': {'name': 'Bob Smith'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(len(threads), 1)
        self.assertEqual(threads[0].patch_set, 1)
        expected_markdown = textwrap.dedent("""\
            # Comment 1 (John Doe)

            Thread 1 root

            # Comment 2 (Jane Roe)

            Thread 1 reply""")
        self.assertEqual(threads[0].thread_markdown, expected_markdown)

    def test_missing_parent_omitted(self):
        comments = [
            {
                'id': 'c2',
                'patch_set': 2,
                'in_reply_to': 'c1',  # c1 is missing
                'message': 'Done',
                'updated': '2013-02-26 15:40:45.000000000',
                'author': {'name': 'Jane Roe'},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(threads, [])

    def test_unknown_author(self):
        comments = [
            {
                'id': 'c1',
                'patch_set': 1,
                'message': 'Nit: whitespace',
                'updated': '2013-02-26 15:40:43.000000000',
                'author': {'name': 'John Doe'},
            },
            {
                'id': 'c2',
                'patch_set': 2,
                'in_reply_to': 'c1',
                'message': 'Done',
                'updated': '2013-02-26 15:40:45.000000000',
                # No author name
                'author': {},
            },
        ]
        threads = gerrit_steps._reconstruct_threads_for_file('foo.cc', comments)
        self.assertEqual(len(threads), 1)
        self.assertEqual(
            threads[0].thread_markdown,
            textwrap.dedent("""\
                # Comment 1 (John Doe)

                Nit: whitespace

                # Comment 2 (Unknown)

                Done"""),
        )


class FetchCommentsForClTest(unittest.TestCase):
    def setUp(self):
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(
                2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
            ),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_session_class = mock.patch('requests.Session').start()
        self.mock_get = self.mock_session_class.return_value.get
        self.mock_sleep = mock.patch('time.sleep').start()

        self.mock_authenticator = mock.Mock()
        mock.patch(
            'gerrit_util._Authenticator.get',
            return_value=self.mock_authenticator,
        ).start()

        self.addCleanup(mock.patch.stopall)

        self.manager = gerrit_steps._SessionManager(
            'chromium-review.googlesource.com'
        )
        self.manager.register_session_for_current_thread()

    def test_success(self):
        mock_response = mock.Mock()
        mock_response.text = textwrap.dedent("""\
            )]}'
            {
              "foo.cc": [
                {
                  "patch_set": 1,
                  "id": "c1",
                  "message": "Nit",
                  "updated": "2013-02-26 15:40:43.000000000",
                  "author": {"name": "John Doe"}
                },
                {
                  "patch_set": 1,
                  "id": "c2",
                  "in_reply_to": "c1",
                  "message": "Ack",
                  "updated": "2013-02-26 15:40:44.000000000",
                  "author": {"name": "Jane Roe"}
                }
              ]
            }""")
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        success = gerrit_steps._fetch_comments_for_cl(
            self.manager, self.cl_info
        )

        self.assertTrue(success)
        self.assertEqual(len(self.cl_info.comments), 1)
        self.assertEqual(self.cl_info.comments[0].file_path, 'foo.cc')
        self.assertEqual(
            self.cl_info.comments[0].thread_markdown,
            textwrap.dedent("""\
                # Comment 1 (John Doe)

                Nit

                # Comment 2 (Jane Roe)

                Ack"""),
        )
        self.mock_get.assert_called_once_with(
            'https://chromium-review.googlesource.com/a/changes/1234/comments',
            timeout=30,
        )

    def test_failure_returns_false(self):
        self.mock_get.side_effect = requests.exceptions.ConnectionError(
            'Connection aborted'
        )

        with self.assertLogs(level='WARNING') as log:
            success = gerrit_steps._fetch_comments_for_cl(
                self.manager, self.cl_info
            )

        self.assertFalse(success)
        self.assertEqual(self.cl_info.comments, [])
        self.mock_get.assert_called_once()
        self.assertTrue(
            any('Failed to fetch comments' in line for line in log.output)
        )

    def test_bad_json_propagates(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n{invalid json}"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(json.JSONDecodeError):
            gerrit_steps._fetch_comments_for_cl(self.manager, self.cl_info)

    def test_not_a_dict_raises(self):
        mock_response = mock.Mock()
        mock_response.text = ")]}'\n[]"
        mock_response.status_code = 200
        self.mock_get.return_value = mock_response

        with self.assertRaises(ValueError) as cm:
            gerrit_steps._fetch_comments_for_cl(self.manager, self.cl_info)

        self.assertIn('Expected dict of comments', str(cm.exception))


class RetrieveCommentsTest(unittest.TestCase):
    def setUp(self):
        self.common_args = CommonArgs(
            project='chromium',
            repo='chromium/src',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(
                2026, 6, 2, 12, 0, 0, tzinfo=datetime.timezone.utc
            ),
            dryrun=False,
            previous_run=None,
        )
        self.cl_info = ClInfo(
            revision='deadbeef',
            cl_number=1234,
            commit_time=datetime.datetime(
                2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
            ),
            commit_position=100,
            description='Test CL',
            dir_metadata=mock.Mock(),
        )
        self.mock_fetch = mock.patch(
            'gerrit_steps._fetch_comments_for_cl'
        ).start()
        self.addCleanup(mock.patch.stopall)

    def test_empty(self):
        gerrit_steps.retrieve_comments(self.common_args, [])
        self.mock_fetch.assert_not_called()

    def test_multiple_success(self):
        cl_infos = [
            self.cl_info,
            ClInfo(
                revision='beefdead',
                cl_number=5678,
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=101,
                description='Test CL 2',
                dir_metadata=mock.Mock(),
            ),
        ]
        t1 = CommentThread('a.cc', 1, 'thread1')
        t2 = CommentThread('b.cc', 2, 'thread2')

        def side_effect(_manager, cl_info):
            if cl_info.cl_number == 1234:
                cl_info.comments = [t1]
            elif cl_info.cl_number == 5678:
                cl_info.comments = [t2]
            return True

        self.mock_fetch.side_effect = side_effect

        gerrit_steps.retrieve_comments(self.common_args, cl_infos)

        self.assertEqual(cl_infos[0].comments, [t1])
        self.assertEqual(cl_infos[1].comments, [t2])
        self.assertEqual(self.mock_fetch.call_count, 2)

    def test_under_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            )
            for i in range(100)
        ]

        self.mock_fetch.side_effect = [False] + [True] * 99

        gerrit_steps.retrieve_comments(self.common_args, cl_infos)
        self.assertEqual(cl_infos[0].comments, [])

    def test_over_threshold(self):
        cl_infos = [
            ClInfo(
                revision=f'rev_{i}',
                cl_number=i,
                commit_time=datetime.datetime(
                    2026, 6, 2, 11, 0, 0, tzinfo=datetime.timezone.utc
                ),
                commit_position=i,
                description=f'Test CL {i}',
                dir_metadata=mock.Mock(),
            )
            for i in range(100)
        ]

        self.mock_fetch.side_effect = [False, False] + [True] * 98

        with self.assertRaises(RuntimeError) as cm:
            gerrit_steps.retrieve_comments(self.common_args, cl_infos)

        self.assertIn('Comment retrieval failure rate', str(cm.exception))


if __name__ == '__main__':
    unittest.main()

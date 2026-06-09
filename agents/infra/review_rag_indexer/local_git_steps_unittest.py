#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for local_git_steps.py."""

import datetime
import subprocess
import textwrap
import unittest
from unittest import mock

from common_types import ClInfo, CommonArgs, PreviousRunInfo
from metadata_tree import MetadataTree
import local_git_steps

# pylint: disable=protected-access


class ParseGitLogOutputTest(unittest.TestCase):

    def test_parse(self):
        sha1 = 'a' * 40
        sha2 = 'b' * 40
        output = textwrap.dedent(f"""\
            {sha1}
            file1.cc
            file2.cc
            {sha2}
            file3.cc
            """)
        result = local_git_steps._parse_git_log_output(output)
        self.assertEqual(result, [
            local_git_steps._RevisionAndChangedFiles(
                revision=sha1, changed_files=['file1.cc', 'file2.cc']),
            local_git_steps._RevisionAndChangedFiles(
                revision=sha2, changed_files=['file3.cc'])
        ])


class ExtractClInfoTest(unittest.TestCase):

    def setUp(self):
        super().setUp()
        self.mock_check_output = mock.patch('subprocess.check_output').start()
        self.addCleanup(mock.patch.stopall)

    def test_extract_cl_info_success(self):
        self.mock_check_output.return_value = textwrap.dedent("""\
            1717448400
            This is a commit message

            Change-Id: I8d32dfe70ab3a533ec82eecac99ccebc07b610ea
            Cr-Commit-Position: refs/heads/main@{#1641065}
            """)

        info = local_git_steps._extract_cl_info('rev1')

        self.assertIsInstance(info, ClInfo)
        self.assertEqual(info.revision, 'rev1')
        self.assertEqual(info.change_id,
                         'I8d32dfe70ab3a533ec82eecac99ccebc07b610ea')
        self.assertEqual(
            info.commit_time,
            datetime.datetime(2024,
                              6,
                              3,
                              21,
                              0,
                              0,
                              tzinfo=datetime.timezone.utc))
        self.assertEqual(info.commit_position, 1641065)
        self.assertIn('This is a commit message', info.description)
        self.assertIsInstance(info.dir_metadata, MetadataTree)
        self.assertIsNone(info.dir_metadata.root.metadata)
        self.assertEqual(info.dir_metadata.root.children, {})

        self.mock_check_output.assert_called_once_with(
            ['git', 'show', '-s', '--format=%ct%n%B', 'rev1'],
            encoding='utf-8')

    def test_extract_cl_info_multiple_change_ids(self):
        self.mock_check_output.return_value = textwrap.dedent("""\
            1717448400
            Revert commit

            Change-Id: I1111111111111111111111111111111111111111
            Original Change-Id: I2222222222222222222222222222222222222222
            Change-Id: I3333333333333333333333333333333333333333
            Cr-Commit-Position: refs/heads/main@{#12345}
            """)
        info = local_git_steps._extract_cl_info('rev1')
        self.assertIsInstance(info, ClInfo)
        self.assertEqual(info.change_id,
                         'I3333333333333333333333333333333333333333')
        self.assertEqual(info.commit_position, 12345)

    def test_extract_cl_info_missing_commit_position(self):
        self.mock_check_output.return_value = textwrap.dedent("""\
            1717448400
            This is a commit message without commit position

            Change-Id: I8d32dfe70ab3a533ec82eecac99ccebc07b610ea
            """)
        with self.assertRaises(ValueError) as cm:
            local_git_steps._extract_cl_info('rev1')
        self.assertIn('Cr-Commit-Position not found in commit description',
                      str(cm.exception))

    def test_extract_cl_info_missing_change_id(self):
        self.mock_check_output.return_value = textwrap.dedent("""\
            1717448400
            This is a commit message without change ID

            Cr-Commit-Position: refs/heads/main@{#1641065}
            """)
        with self.assertRaises(ValueError) as cm:
            local_git_steps._extract_cl_info('rev1')
        self.assertIn('Change-Id not found in commit description',
                      str(cm.exception))


class GetCommitsToProcessTest(unittest.TestCase):

    def setUp(self):
        super().setUp()
        self.mock_check_output = mock.patch('subprocess.check_output').start()
        self.mock_parse = mock.patch(
            'local_git_steps._parse_git_log_output').start()
        self.addCleanup(mock.patch.stopall)

    def test_get_commits_to_process_clobber(self):
        common_args = CommonArgs(project='proj',
                                 repo='repo',
                                 window=datetime.timedelta(days=1),
                                 window_base=datetime.datetime(
                                     2026,
                                     6,
                                     2,
                                     12,
                                     0,
                                     0,
                                     tzinfo=datetime.timezone.utc),
                                 dryrun=False,
                                 previous_run=None)
        self.mock_check_output.return_value = 'git log output'
        self.mock_parse.return_value = [
            local_git_steps._RevisionAndChangedFiles(revision='rev1',
                                                     changed_files=['file1'])
        ]

        commits = local_git_steps._get_commits_to_process(common_args)

        self.assertEqual(commits, [
            local_git_steps._RevisionAndChangedFiles(revision='rev1',
                                                     changed_files=['file1'])
        ])
        expected_since = '2026-06-01T12:00:00+00:00'
        self.mock_check_output.assert_called_once_with([
            'git', 'log', '--format=%H', '--name-only', '--reverse',
            f'--since={expected_since}'
        ],
                                                       encoding='utf-8')
        self.mock_parse.assert_called_once_with('git log output')

    def test_get_commits_to_process_incremental(self):
        previous_run = PreviousRunInfo(revision='prev_rev',
                                       start_time=datetime.datetime(
                                           2026,
                                           6,
                                           1,
                                           12,
                                           0,
                                           0,
                                           tzinfo=datetime.timezone.utc))
        common_args = CommonArgs(project='proj',
                                 repo='repo',
                                 window=datetime.timedelta(days=1),
                                 window_base=datetime.datetime(
                                     2026,
                                     6,
                                     2,
                                     12,
                                     0,
                                     0,
                                     tzinfo=datetime.timezone.utc),
                                 dryrun=False,
                                 previous_run=previous_run)
        self.mock_check_output.return_value = 'git log output'
        self.mock_parse.return_value = [
            local_git_steps._RevisionAndChangedFiles(revision='rev1',
                                                     changed_files=['file1'])
        ]

        commits = local_git_steps._get_commits_to_process(common_args)

        self.assertEqual(commits, [
            local_git_steps._RevisionAndChangedFiles(revision='rev1',
                                                     changed_files=['file1'])
        ])
        self.mock_check_output.assert_called_once_with([
            'git', 'log', '--format=%H', '--name-only', '--reverse',
            'prev_rev..HEAD'
        ],
                                                       encoding='utf-8')

    def test_get_commits_to_process_failure(self):
        common_args = CommonArgs(project='proj',
                                 repo='repo',
                                 window=datetime.timedelta(days=1),
                                 window_base=datetime.datetime(
                                     2026,
                                     6,
                                     2,
                                     12,
                                     0,
                                     0,
                                     tzinfo=datetime.timezone.utc),
                                 dryrun=False,
                                 previous_run=None)
        self.mock_check_output.side_effect = subprocess.CalledProcessError(
            returncode=1, cmd='git log')

        with self.assertRaises(subprocess.CalledProcessError):
            local_git_steps._get_commits_to_process(common_args)


class ProcessRevisionsTest(unittest.TestCase):

    def setUp(self):
        super().setUp()
        self.mock_revision_exists = mock.patch(
            'local_git_steps.git_utils.revision_exists').start()
        self.mock_extract_cl_info = mock.patch(
            'local_git_steps._extract_cl_info').start()
        self.mock_read_files = mock.patch(
            'local_git_steps.git_utils.read_files_at_revision').start()
        self.mock_metadata_read_files = mock.patch(
            'metadata_tree.read_files_at_revision').start()
        self.mock_metadata_read_files.side_effect = self.mock_read_files
        self.addCleanup(mock.patch.stopall)

    def test_process_commits(self):
        self.mock_revision_exists.return_value = True

        cl_infos = {
            'commit1~1': {
                'revision':
                'commit1~1',
                'change_id':
                'Iinit',
                'commit_time':
                datetime.datetime(2026,
                                  6,
                                  3,
                                  20,
                                  0,
                                  0,
                                  tzinfo=datetime.timezone.utc),
                'commit_position':
                99,
                'description':
                'Init commit',
            },
            'commit1': {
                'revision':
                'commit1',
                'change_id':
                'I1111',
                'commit_time':
                datetime.datetime(2026,
                                  6,
                                  3,
                                  20,
                                  1,
                                  0,
                                  tzinfo=datetime.timezone.utc),
                'commit_position':
                100,
                'description':
                'Commit 1',
            },
            'commit2': {
                'revision':
                'commit2',
                'change_id':
                'I2222',
                'commit_time':
                datetime.datetime(2026,
                                  6,
                                  3,
                                  20,
                                  2,
                                  0,
                                  tzinfo=datetime.timezone.utc),
                'commit_position':
                101,
                'description':
                'Commit 2',
            },
            'commit3': {
                'revision':
                'commit3',
                'change_id':
                'I3333',
                'commit_time':
                datetime.datetime(2026,
                                  6,
                                  3,
                                  20,
                                  3,
                                  0,
                                  tzinfo=datetime.timezone.utc),
                'commit_position':
                102,
                'description':
                'Commit 3',
            },
        }
        self.mock_extract_cl_info.side_effect = (
            lambda rev: ClInfo(dir_metadata=MetadataTree(), **cl_infos[rev]))

        self.mock_read_files.return_value = {
            'foo/DIR_METADATA': 'component: "NewFoo"'
        }

        initial_tree = MetadataTree()
        initial_tree.insert('foo/DIR_METADATA', {'component': 'Foo'})
        parsed_files = {'foo/DIR_METADATA': {'component': 'Foo'}}
        dir_metadata_paths = {'foo/DIR_METADATA'}

        commits = [
            local_git_steps._RevisionAndChangedFiles(
                revision='commit1', changed_files=['some/file.cc']),
            local_git_steps._RevisionAndChangedFiles(
                revision='commit2', changed_files=['foo/DIR_METADATA']),
            local_git_steps._RevisionAndChangedFiles(
                revision='commit3', changed_files=['some/other_file.cc']),
        ]

        cl_objects = local_git_steps._process_commits(commits, initial_tree,
                                                      parsed_files,
                                                      dir_metadata_paths)

        self.assertEqual(len(cl_objects), 3)
        self.assertEqual(cl_objects[0].revision, 'commit1')
        self.assertEqual(
            cl_objects[0].dir_metadata.get_metadata('foo/file.cc'),
            {'component': 'Foo'})

        self.assertEqual(cl_objects[1].revision, 'commit2')
        self.assertEqual(
            cl_objects[1].dir_metadata.get_metadata('foo/file.cc'),
            {'component': 'NewFoo'})

        self.assertEqual(cl_objects[2].revision, 'commit3')
        self.assertEqual(
            cl_objects[2].dir_metadata.get_metadata('foo/file.cc'),
            {'component': 'NewFoo'})

    def test_process_commits_missing_cp_raises_error(self):
        self.mock_revision_exists.return_value = True
        self.mock_extract_cl_info.side_effect = ValueError('Missing CP')

        initial_tree = MetadataTree()
        parsed_files = {}
        dir_metadata_paths = set()
        commits = [
            local_git_steps._RevisionAndChangedFiles(
                revision='commit1', changed_files=['some/file.cc'])
        ]

        with self.assertRaises(ValueError):
            local_git_steps._process_commits(commits, initial_tree,
                                             parsed_files, dir_metadata_paths)

    def test_process_commits_git_error_raises_error(self):
        self.mock_revision_exists.return_value = True
        cl_info = {
            'revision':
            'commit1',
            'change_id':
            'I1111',
            'commit_time':
            datetime.datetime(2026,
                              6,
                              3,
                              20,
                              1,
                              0,
                              tzinfo=datetime.timezone.utc),
            'commit_position':
            100,
            'description':
            'Commit 1',
        }

        def side_effect(rev):
            if rev == 'commit1~1':
                return ClInfo(
                    revision='commit1~1',
                    change_id='Iinit',
                    commit_time=datetime.datetime(
                        2026, 6, 3, 20, 0, 0, tzinfo=datetime.timezone.utc),
                    commit_position=99,
                    description='Init commit',
                    dir_metadata=MetadataTree(),
                )
            return ClInfo(dir_metadata=MetadataTree(), **cl_info)

        self.mock_extract_cl_info.side_effect = side_effect
        self.mock_read_files.side_effect = Exception('Git error')

        initial_tree = MetadataTree()
        parsed_files = {'foo/DIR_METADATA': {'component': 'Foo'}}
        dir_metadata_paths = {'foo/DIR_METADATA'}
        commits = [
            local_git_steps._RevisionAndChangedFiles(
                revision='commit1', changed_files=['foo/DIR_METADATA'])
        ]

        with self.assertRaises(Exception) as context:
            local_git_steps._process_commits(commits, initial_tree,
                                             parsed_files, dir_metadata_paths)
        self.assertEqual(str(context.exception), 'Git error')

    def test_process_commits_malformed_metadata_raises_error(self):
        self.mock_revision_exists.return_value = True
        cl_info = {
            'revision':
            'commit1',
            'change_id':
            'I1111',
            'commit_time':
            datetime.datetime(2026,
                              6,
                              3,
                              20,
                              1,
                              0,
                              tzinfo=datetime.timezone.utc),
            'commit_position':
            100,
            'description':
            'Commit 1',
        }

        def side_effect(rev):
            if rev == 'commit1~1':
                return ClInfo(
                    revision='commit1~1',
                    change_id='Iinit',
                    commit_time=datetime.datetime(
                        2026, 6, 3, 20, 0, 0, tzinfo=datetime.timezone.utc),
                    commit_position=99,
                    description='Init commit',
                    dir_metadata=MetadataTree(),
                )
            return ClInfo(dir_metadata=MetadataTree(), **cl_info)

        self.mock_extract_cl_info.side_effect = side_effect
        self.mock_read_files.return_value = {'foo/DIR_METADATA': '@invalid'}

        initial_tree = MetadataTree()
        parsed_files = {'foo/DIR_METADATA': {'component': 'Foo'}}
        dir_metadata_paths = {'foo/DIR_METADATA'}
        commits = [
            local_git_steps._RevisionAndChangedFiles(
                revision='commit1', changed_files=['foo/DIR_METADATA'])
        ]

        with self.assertRaises(Exception):
            local_git_steps._process_commits(commits, initial_tree,
                                             parsed_files, dir_metadata_paths)

    def test_process_commits_malformed_mixin_raises_error(self):
        self.mock_revision_exists.return_value = True
        cl_info = {
            'revision':
            'commit1',
            'change_id':
            'I1111',
            'commit_time':
            datetime.datetime(2026,
                              6,
                              3,
                              20,
                              1,
                              0,
                              tzinfo=datetime.timezone.utc),
            'commit_position':
            100,
            'description':
            'Commit 1',
        }

        def side_effect(rev):
            if rev == 'commit1~1':
                return ClInfo(
                    revision='commit1~1',
                    change_id='Iinit',
                    commit_time=datetime.datetime(
                        2026, 6, 3, 20, 0, 0, tzinfo=datetime.timezone.utc),
                    commit_position=99,
                    description='Init commit',
                    dir_metadata=MetadataTree(),
                )
            return ClInfo(dir_metadata=MetadataTree(), **cl_info)

        self.mock_extract_cl_info.side_effect = side_effect

        def read_files_side_effect(_revision, paths):
            if 'foo/DIR_METADATA' in paths:
                return {'foo/DIR_METADATA': 'mixins: "//bar/MIXIN"'}
            if 'bar/MIXIN' in paths:
                return {'bar/MIXIN': '@invalid'}
            return {}

        self.mock_read_files.side_effect = read_files_side_effect

        initial_tree = MetadataTree()
        parsed_files = {'foo/DIR_METADATA': {'component': 'Foo'}}
        dir_metadata_paths = {'foo/DIR_METADATA'}
        commits = [
            local_git_steps._RevisionAndChangedFiles(
                revision='commit1', changed_files=['foo/DIR_METADATA'])
        ]

        with self.assertRaises(Exception):
            local_git_steps._process_commits(commits, initial_tree,
                                             parsed_files, dir_metadata_paths)


class GetCommitPositionTest(unittest.TestCase):

    def setUp(self):
        super().setUp()
        self.mock_extract = mock.patch(
            'local_git_steps._extract_cl_info').start()
        self.addCleanup(mock.patch.stopall)

    def test_get_commit_position(self):
        self.mock_extract.return_value = mock.Mock(commit_position=123)
        self.assertEqual(local_git_steps.get_commit_position('rev'), 123)
        self.mock_extract.assert_called_once_with('rev')

    def test_get_commit_position_error(self):
        self.mock_extract.side_effect = ValueError('Git error')
        with self.assertRaises(ValueError):
            local_git_steps.get_commit_position('rev')


if __name__ == '__main__':
    unittest.main()

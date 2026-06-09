#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for create_index.py."""

import contextlib
import datetime
import io
import json
import logging
import pathlib
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import create_index

# pylint: disable=protected-access


class CreateIndexTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()
        self.mock_chdir = mock.patch('os.chdir').start()
        self.addCleanup(mock.patch.stopall)

    def test_calculate_time_window(self):
        window, base = create_index._calculate_time_window('1 hour ago')
        self.assertEqual(window.total_seconds(), 3600)
        # base should be very close to current time.
        now = datetime.datetime.now(tz=datetime.timezone.utc)
        self.assertLess(abs((now - base).total_seconds()), 5)

    def test_perform_initial_setup_default(self):
        args = mock.Mock()
        args.verbose = False
        args.working_directory = None

        with mock.patch('logging.basicConfig') as mock_logging_config:
            create_index._perform_initial_setup(args)
            mock_logging_config.assert_called_once_with(level=logging.INFO)
        self.mock_chdir.assert_not_called()

    def test_perform_initial_setup_verbose_and_wdir(self):
        args = mock.Mock()
        args.verbose = True
        args.working_directory = '/fake/wdir'

        with mock.patch('logging.basicConfig') as mock_logging_config:
            create_index._perform_initial_setup(args)
            mock_logging_config.assert_called_once_with(level=logging.DEBUG)
        self.mock_chdir.assert_called_once_with('/fake/wdir')

    @mock.patch('sys.argv', [
        'create_index.py', '--since', 'in 1 hour', '--project', 'proj',
        '--repo', 'repo'
    ])
    @mock.patch('create_index._perform_initial_setup')
    def test_main_negative_window_raises_value_error(self, mock_setup):
        with self.assertRaises(ValueError) as cm:
            create_index.main()

        self.assertIn('resulted in a time window in the future',
                      str(cm.exception))
        mock_setup.assert_called_once()
        called_args = mock_setup.call_args[0][0]
        self.assertEqual(called_args.since, 'in 1 hour')
        self.assertEqual(called_args.project, 'proj')
        self.assertEqual(called_args.repo, 'repo')

    @mock.patch('sys.argv', [
        'create_index.py', '--since', '1 hour ago', '--project', 'proj',
        '--repo', 'repo'
    ])
    @mock.patch('create_index._perform_initial_setup')
    @mock.patch('create_index._retrieve_previous_run_info')
    @mock.patch('create_index.local_git_steps.process_local_git_data')
    def test_main_success(self, mock_process_local_git_data, mock_retrieve,
                          mock_setup):
        mock_process_local_git_data.return_value = []
        approximate_base = datetime.datetime.now(tz=datetime.timezone.utc)

        create_index.main()

        mock_setup.assert_called_once()
        called_args = mock_setup.call_args[0][0]
        self.assertEqual(called_args.since, '1 hour ago')
        self.assertEqual(called_args.project, 'proj')
        self.assertEqual(called_args.repo, 'repo')
        self.assertFalse(called_args.dryrun)

        mock_retrieve.assert_called_once()
        called_args = mock_retrieve.call_args[0][0]
        self.assertIsInstance(called_args, create_index.CommonArgs)
        self.assertEqual(called_args.project, 'proj')
        self.assertEqual(called_args.repo, 'repo')
        self.assertEqual(called_args.window, datetime.timedelta(hours=1))
        self.assertLess(
            abs((approximate_base - called_args.window_base).total_seconds()),
            5)
        self.assertTrue(called_args.clobber)
        self.assertFalse(called_args.dryrun)
        self.assertIsNone(called_args.previous_run)
        self.assertEqual(called_args.head_git_revision, 'HEAD')

        mock_process_local_git_data.assert_called_once_with(called_args)

    @mock.patch('sys.argv', [
        'create_index.py', '--since', '1 hour ago', '--project', 'proj',
        '--repo', 'repo', '--head-git-revision', 'my_head_rev'
    ])
    @mock.patch('create_index._perform_initial_setup')
    @mock.patch('create_index._retrieve_previous_run_info')
    @mock.patch('create_index.local_git_steps.process_local_git_data')
    @mock.patch('create_index.git_utils.revision_exists')
    def test_main_success_with_head_git_revision(self, mock_revision_exists,
                                                 mock_process_local_git_data,
                                                 mock_retrieve, mock_setup):
        mock_revision_exists.return_value = True
        mock_process_local_git_data.return_value = []

        create_index.main()

        mock_setup.assert_called_once()
        mock_revision_exists.assert_called_once_with('my_head_rev')
        mock_retrieve.assert_called_once()
        called_args = mock_retrieve.call_args[0][0]
        self.assertIsInstance(called_args, create_index.CommonArgs)
        self.assertEqual(called_args.head_git_revision, 'my_head_rev')

    @mock.patch('sys.argv', [
        'create_index.py', '--since', '1 hour ago', '--project', 'proj',
        '--repo', 'repo', '--head-git-revision', 'invalid_rev'
    ])
    @mock.patch('create_index._perform_initial_setup')
    @mock.patch('create_index.git_utils.revision_exists')
    def test_main_invalid_head_git_revision_fails_validation(
            self, mock_revision_exists, mock_setup):
        mock_revision_exists.return_value = False

        with (self.assertRaises(SystemExit),
              contextlib.redirect_stderr(io.StringIO()) as stderr):
            create_index.main()

        self.assertTrue(
            'Invalid head git revision: invalid_rev' in stderr.getvalue())
        mock_setup.assert_not_called()
        mock_revision_exists.assert_called_once_with('invalid_rev')


class CreateIndexRetrievePreviousRunInfoTest(fake_filesystem_unittest.TestCase
                                             ):

    def setUp(self):
        self.setUpPyfakefs()

        self.mock_run = mock.patch('subprocess.run').start()
        self.mock_install = mock.patch('create_index.cipd_helpers'
                                       '.install_package').start()
        self.mock_install.side_effect = self._fake_install

        self.manifest_content = None

        self.addCleanup(mock.patch.stopall)

    def _fake_install(self, _package, _version, cipd_root):
        if self.manifest_content is not None:
            manifest_path = cipd_root / 'manifest.json'
            manifest_path.write_text(json.dumps(self.manifest_content),
                                     encoding='utf-8')
            return True
        return False

    def _get_good_manifest_content(self):
        return {
            'script_version':
            1,
            'window_seconds':
            86400,
            'start_time':
            datetime.datetime(2026,
                              6,
                              1,
                              12,
                              0,
                              0,
                              tzinfo=datetime.timezone.utc).timestamp(),
            'revision':
            'deadbeef'
        }

    def test_retrieve_previous_run_info_install_failed(self):
        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)

        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('Failed to retrieve manifest' in line for line in log.output))
        self.assertIsNone(common_args.previous_run)

        self.mock_install.assert_called_once()
        args, _ = self.mock_install.call_args
        self.assertEqual(args[0], 'infra/review_rag/proj/repo/manifest')
        self.assertEqual(args[1], 'latest')
        self.assertIsInstance(args[2], pathlib.Path)

    def test_retrieve_previous_run_info_version_mismatch(self):
        self.manifest_content = self._get_good_manifest_content()
        self.manifest_content['script_version'] = -1

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)

        self.assertTrue(common_args.clobber)
        self.assertTrue(any('does not match' in line for line in log.output))
        self.assertIsNone(common_args.previous_run)

        self.mock_install.assert_called_once()
        self.assertIsInstance(self.mock_install.call_args[0][2], pathlib.Path)

    def test_retrieve_previous_run_info_window_seconds_missing(self):
        self.manifest_content = self._get_good_manifest_content()
        del self.manifest_content['window_seconds']

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)
        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('did not report a window' in line for line in log.output))

    def test_retrieve_previous_run_info_window_seconds_mismatch(self):
        self.manifest_content = self._get_good_manifest_content()
        self.manifest_content['window_seconds'] = 43200

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)
        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('reported a window of' in line for line in log.output))

    def test_retrieve_previous_run_info_start_time_missing(self):
        self.manifest_content = self._get_good_manifest_content()
        del self.manifest_content['start_time']

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)
        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('did not report a start time' in line for line in log.output))

    def test_retrieve_previous_run_info_no_overlap(self):
        self.manifest_content = self._get_good_manifest_content()
        # Set start_time to 2026-05-31T12:00:00Z (2 days before base)
        start_time = datetime.datetime(2026,
                                       5,
                                       31,
                                       12,
                                       0,
                                       0,
                                       tzinfo=datetime.timezone.utc)
        self.manifest_content['start_time'] = start_time.timestamp()

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)
        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('no overlap with the current window' in line
                for line in log.output))

    def test_retrieve_previous_run_info_revision_missing(self):
        self.manifest_content = self._get_good_manifest_content()
        del self.manifest_content['revision']

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)
        self.assertTrue(common_args.clobber)
        self.assertTrue(
            any('did not contain a revision' in line for line in log.output))

    def test_retrieve_previous_run_info_valid(self):
        self.manifest_content = self._get_good_manifest_content()

        common_args = create_index.CommonArgs(
            project='proj',
            repo='repo',
            window=datetime.timedelta(days=1),
            window_base=datetime.datetime(2026,
                                          6,
                                          2,
                                          12,
                                          0,
                                          0,
                                          tzinfo=datetime.timezone.utc),
            dryrun=False,
            previous_run=None)

        with self.assertLogs(level='INFO') as log:
            create_index._retrieve_previous_run_info(common_args)

        self.assertIn(
            "INFO:root:Last run's manifest appears to be valid and relevant. "
            'Proceeding with incremental index creation', log.output)

        self.assertFalse(common_args.clobber)
        self.assertIsNotNone(common_args.previous_run)
        self.assertEqual(common_args.previous_run.revision, 'deadbeef')
        expected_start_time = datetime.datetime(2026,
                                                6,
                                                1,
                                                12,
                                                0,
                                                0,
                                                tzinfo=datetime.timezone.utc)
        self.assertEqual(common_args.previous_run.start_time,
                         expected_start_time)

        self.mock_install.assert_called_once()
        self.assertIsInstance(self.mock_install.call_args[0][2], pathlib.Path)


if __name__ == '__main__':
    unittest.main()

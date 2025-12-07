#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing publish_package.py."""

import argparse
import unittest
import unittest.mock as mock

import publish_package

_PACKAGES = ['test_package']
_REPO = 'test_repo'


class PublishPackageTest(unittest.TestCase):
    """Unittests for publish_package.py."""

    def setUp(self) -> None:
        self._ffx_patcher = mock.patch('publish_package.run_ffx_command')
        self._ffx_mock = self._ffx_patcher.start()
        self.addCleanup(self._ffx_mock.stop)


    def test_new_repo(self) -> None:
        """Test setting |new_repo| to True in |publish_packages|."""

        publish_package.ensure_repository(
            argparse.Namespace(**{
                'repo': _REPO,
                'no_repo_init': False,
            }))
        publish_package.publish_packages(
            _PACKAGES,
            argparse.Namespace(**{
                'repo': _REPO,
                'no_repo_init': False,
            }))
        self.assertEqual(self._ffx_mock.call_count, 2)
        first_call, second_call = self._ffx_mock.call_args_list
        self.assertEqual(['repository', 'create', _REPO],
                         first_call.kwargs['cmd'])
        self.assertEqual([
            'repository', 'publish', '--package-archive', _PACKAGES[0], _REPO
        ], second_call.kwargs['cmd'])

    @mock.patch('os.path.exists', return_value=False)
    def test_new_repo_if_not_existing(self, *_) -> None:
        """Always initialize the repo if it's not existing."""

        publish_package.ensure_repository(
            argparse.Namespace(**{
                'repo': _REPO,
                'no_repo_init': False,
            }))
        self.assertEqual(self._ffx_mock.call_count, 1)
        first_call = self._ffx_mock.call_args
        self.assertEqual(['repository', 'create', _REPO],
                         first_call.kwargs['cmd'])

    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('os.path.isdir', return_value=True)
    def test_no_new_repo(self, *_) -> None:
        """Test setting |new_repo| to False in |publish_packages|."""

        publish_package.ensure_repository(
            argparse.Namespace(**{
                'repo': _REPO,
                'no_repo_init': True,
            }))
        publish_package.publish_packages(
            _PACKAGES,
            argparse.Namespace(**{
                'repo': _REPO,
                'no_repo_init': True,
            }))
        self.assertEqual(self._ffx_mock.call_count, 1)


    @mock.patch('os.path.exists', return_value=True)
    @mock.patch('os.path.isdir', return_value=False)
    def test_not_a_dir(self, *_) -> None:
        """Test using a non-directory for repo."""

        with self.assertRaises(AssertionError):
            publish_package.ensure_repository(
                argparse.Namespace(**{
                    'repo': _REPO,
                    'no_repo_init': True,
                }))


    def test_allow_temp_repo(self) -> None:
        """Test setting |allow_temp_repo| to True in |register_package_args|."""

        parser = argparse.ArgumentParser()
        publish_package.register_package_args(parser)
        args = parser.parse_args(['--no-repo-init'])
        self.assertEqual(args.no_repo_init, True)


if __name__ == '__main__':
    unittest.main()

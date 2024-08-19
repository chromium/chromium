#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing publish_package.py."""

import argparse
import unittest
import unittest.mock as mock

from io import StringIO

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

        publish_package.publish_packages(_PACKAGES, _REPO, True)
        self.assertEqual(self._ffx_mock.call_count, 2)
        first_call, second_call = self._ffx_mock.call_args_list
        self.assertEqual(['repository', 'create', _REPO],
                         first_call.kwargs['cmd'])
        self.assertEqual([
            'repository', 'publish', '--package-archive', _PACKAGES[0], _REPO
        ], second_call.kwargs['cmd'])

    def test_no_new_repo(self) -> None:
        """Test setting |new_repo| to False in |publish_packages|."""

        publish_package.publish_packages(['test_package'], 'test_repo', False)
        self.assertEqual(self._ffx_mock.call_count, 1)


    def test_allow_temp_repo(self) -> None:
        """Test setting |allow_temp_repo| to True in |register_package_args|."""

        parser = argparse.ArgumentParser()
        publish_package.register_package_args(parser, True)
        args = parser.parse_args(['--no-repo-init'])
        self.assertEqual(args.no_repo_init, True)

    @mock.patch('sys.stderr', new_callable=StringIO)
    def test_not_allow_temp_repo(self, mock_stderr) -> None:
        """Test setting |allow_temp_repo| to False in
        |register_package_args|."""

        parser = argparse.ArgumentParser()
        publish_package.register_package_args(parser)
        with self.assertRaises(SystemExit):
            parser.parse_args(['--no-repo-init'])
        self.assertRegex(mock_stderr.getvalue(), 'unrecognized arguments')


if __name__ == '__main__':
    unittest.main()

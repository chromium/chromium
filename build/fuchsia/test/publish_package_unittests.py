#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing publish_package.py."""

import argparse
import unittest
import unittest.mock as mock

from io import StringIO

import publish_package


class PublishPackageTest(unittest.TestCase):
    """Unittests for publish_package.py."""

    def setUp(self) -> None:
        self._subprocess_patcher = mock.patch('publish_package.subprocess.run')
        self._subprocess_mock = self._subprocess_patcher.start()
        self.addCleanup(self._subprocess_mock.stop)

    def test_new_repo(self) -> None:
        """Test setting |new_repo| to True in |publish_packages|."""

        publish_package.publish_packages(['test_package'], 'test_repo', True)
        self.assertEqual(self._subprocess_mock.call_count, 2)
        first_call = self._subprocess_mock.call_args_list[0]
        self.assertEqual('newrepo', first_call[0][0][1])

    def test_no_new_repo(self) -> None:
        """Test setting |new_repo| to False in |publish_packages|."""

        publish_package.publish_packages(['test_package'], 'test_repo', False)
        self.assertEqual(self._subprocess_mock.call_count, 1)
        first_call = self._subprocess_mock.call_args_list[0]
        self.assertEqual('publish', first_call[0][0][1])

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

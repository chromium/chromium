#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""File for testing deploy_to_fuchsia.py."""

import os
import unittest
import unittest.mock as mock

import deploy_to_fuchsia


class DeployToFuchsiaTest(unittest.TestCase):
    """Unittests for deploy_to_fuchsia.py."""

    @mock.patch('deploy_to_fuchsia.read_package_paths', return_value=[])
    @mock.patch('deploy_to_fuchsia.publish_packages')
    @mock.patch('deploy_to_fuchsia.install_symbols')
    def test_main(self, mock_install, mock_publish, mock_read) -> None:
        """Tests |main|."""

        test_package = 'test_package'
        fuchsia_out_dir = 'out/fuchsia'
        with mock.patch('sys.argv', [
                'deploy_to_fuchsia.py', test_package, '-C', 'out/chromium',
                '--fuchsia-out-dir', fuchsia_out_dir
        ]):
            deploy_to_fuchsia.main()
            self.assertEqual(mock_read.call_args_list[0][0][1], test_package)
            self.assertEqual(mock_publish.call_args_list[0][0][1],
                             os.path.join(fuchsia_out_dir, 'amber-files'))
            self.assertEqual(mock_install.call_args_list[0][0][1],
                             fuchsia_out_dir)


if __name__ == '__main__':
    unittest.main()

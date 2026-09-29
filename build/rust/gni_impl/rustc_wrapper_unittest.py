#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest import mock

sys.path.append(os.path.dirname(__file__))
from rustc_wrapper import PrepareRustEnvForExecution


class PrepareRustEnvForExecutionTest(unittest.TestCase):
    def test_prepends_active_python_dir_to_existing_path(self) -> None:
        env = {'PATH': '/usr/bin:/bin'}
        expected_python_dir = os.path.dirname(os.path.abspath(sys.executable))

        PrepareRustEnvForExecution(env)

        self.assertEqual(
            env['PATH'],
            f'{expected_python_dir}{os.pathsep}/usr/bin:/bin',
        )

    @mock.patch.dict(os.environ, {}, clear=True)
    def test_prepends_active_python_dir_when_path_unset(self) -> None:
        env: dict[str, str] = {}
        expected_python_dir = os.path.dirname(os.path.abspath(sys.executable))

        PrepareRustEnvForExecution(env)

        self.assertEqual(
            env['PATH'],
            f'{expected_python_dir}{os.pathsep}{os.defpath}',
        )


if __name__ == '__main__':
    unittest.main()

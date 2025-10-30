#! /usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access
"""Unit tests for remote_output_manager.py.

Example usage:
  vpython3 remote_output_manager_test.py
"""

from pathlib import Path
import sys
import unittest

build_android_path = Path(__file__).parents[2]
sys.path.append(str(build_android_path))

lib_path_root = Path(__file__).parents[3] / 'lib'
sys.path.append(str(lib_path_root))

from pylib.base import output_manager
from pylib.base import output_manager_test_case
from pylib.output import remote_output_manager

import mock  # pylint: disable=import-error


@mock.patch('lib.common.google_storage_helper')
class RemoteOutputManagerTest(output_manager_test_case.OutputManagerTestCase):

  def setUp(self):
    self._output_manager = remote_output_manager.RemoteOutputManager(
        'this-is-a-fake-bucket')

  def testUsableTempFile(self, google_storage_helper_mock):
    del google_storage_helper_mock
    self.assertUsableTempFile(
        self._output_manager._CreateArchivedFile('test_file', 'test_subdir',
                                                 output_manager.Datatype.TEXT,
                                                 None))


if __name__ == '__main__':
  unittest.main()

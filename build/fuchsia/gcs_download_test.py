#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import tarfile
import unittest
from unittest import mock

from gcs_download import DownloadAndUnpackFromCloudStorage


def _mock_task(status_code: int = 0, stderr: str = '') -> mock.Mock:
  task_mock = mock.Mock()
  attrs = {
      'returncode': status_code,
      'wait.return_value': status_code,
      'communicate.return_value': (None, stderr.encode()),
  }
  task_mock.configure_mock(**attrs)

  return task_mock


@mock.patch('subprocess.Popen')
@mock.patch('tarfile.open')
class TestDownloadAndUnpackFromCloudStorage(unittest.TestCase):
  def testHappyPath(self, mock_tarfile, mock_popen):
    mock_popen.return_value = _mock_task()

    DownloadAndUnpackFromCloudStorage('', '')

  def testFailedTarOpen(self, mock_tarfile, mock_popen):
    mock_popen.return_value = _mock_task(stderr='some error')
    mock_tarfile.side_effect = tarfile.ReadError()

    with self.assertRaises(subprocess.CalledProcessError):
      DownloadAndUnpackFromCloudStorage('', '')

  def testBadTaskStatusCode(self, mock_tarfile, mock_popen):
    mock_popen.return_value = _mock_task(stderr='some error', status_code=1)

    with self.assertRaises(subprocess.CalledProcessError):
      DownloadAndUnpackFromCloudStorage('', '')


if __name__ == '__main__':
  unittest.main()

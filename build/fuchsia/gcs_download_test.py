#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys
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


@mock.patch('tempfile.TemporaryDirectory')
@mock.patch('subprocess.run')
@mock.patch('tarfile.open')
class TestDownloadAndUnpackFromCloudStorage(unittest.TestCase):
  def testHappyPath(self, mock_tarfile, mock_run, mock_tmp_dir):
    mock_run.return_value = _mock_task()
    mock_tmp_dir.return_value.__enter__.return_value = '/some/tmp/dir'

    mock_seq = mock.Mock()
    mock_seq.attach_mock(mock_run, 'Run')
    mock_seq.attach_mock(mock_tarfile, 'Untar')
    mock_seq.attach_mock(mock_tmp_dir, 'MkTmpD')

    DownloadAndUnpackFromCloudStorage('gs://some/url', 'output/dir')

    mock_seq.assert_has_calls([
        mock.call.MkTmpD(),
        mock.call.MkTmpD().__enter__(),
        mock.call.Run(mock.ANY,
                      stderr=subprocess.PIPE,
                      stdout=subprocess.PIPE,
                      check=True,
                      encoding='utf-8'),
        mock.call.Untar(name='/some/tmp/dir/image.tgz', mode='r|gz'),
        mock.call.Untar().extractall(path='output/dir'),
        mock.call.MkTmpD().__exit__(None, None, None)
    ],
                              any_order=False)

    # Verify cmd.
    cmd = ' '.join(mock_run.call_args[0][0])
    self.assertRegex(
        cmd,
        r'.*python\s.*gsutil.py\s+cp\s+gs://some/url\s+/some/tmp/dir/image.tgz')

  def testFailedTarOpen(self, mock_tarfile, mock_run, mock_tmp_dir):
    mock_run.return_value = _mock_task(stderr='some error')
    mock_tarfile.side_effect = tarfile.ReadError()

    with self.assertRaises(subprocess.CalledProcessError):
      DownloadAndUnpackFromCloudStorage('', '')
      mock_tmp_dir.assert_called_once()
      mock_run.assert_called_once()
      mock_tarfile.assert_called_once()

  def testBadTaskStatusCode(self, mock_tarfile, mock_run, mock_tmp_dir):
    mock_run.side_effect = subprocess.CalledProcessError(cmd='some/command',
                                                         returncode=1)

    with self.assertRaises(subprocess.CalledProcessError):
      DownloadAndUnpackFromCloudStorage('', '')
      mock_run.assert_called_once()
      mock_tarfile.assert_not_called()
      mock_tmp_dir.assert_called_once()


if __name__ == '__main__':
  unittest.main()

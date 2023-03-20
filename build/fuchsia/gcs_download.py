# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import tarfile
import tempfile

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))

from common import DIR_SRC_ROOT

sys.path.append(os.path.join(DIR_SRC_ROOT, 'build'))
import find_depot_tools


def DownloadAndUnpackFromCloudStorage(url, output_dir):
  """Fetches a tarball from GCS and uncompresses it to |output_dir|."""

  # Pass the compressed stream directly to 'tarfile'; don't bother writing it
  # to disk first.
  tmp_file = 'image.tgz'
  with tempfile.TemporaryDirectory() as tmp_d:
    tmp_file_location = os.path.join(tmp_d, tmp_file)
    cmd = [
        sys.executable,
        os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'cp', url,
        tmp_file_location
    ]

    logging.debug('Running "%s"', ' '.join(cmd))
    task = subprocess.run(cmd,
                          stderr=subprocess.PIPE,
                          stdout=subprocess.PIPE,
                          check=True,
                          encoding='utf-8')

    try:
      tarfile.open(name=tmp_file_location,
                   mode='r|gz').extractall(path=output_dir)
    except tarfile.ReadError as exc:
      _, stderr_data = task.communicate()
      stderr_data = stderr_data.decode()
      raise subprocess.CalledProcessError(
          task.returncode, cmd,
          'Failed to read a tarfile from gsutil.py.\n{}'.format(
              stderr_data if stderr_data else '')) from exc

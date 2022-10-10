# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import tarfile

from common import DIR_SOURCE_ROOT

sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build'))
import find_depot_tools


def DownloadAndUnpackFromCloudStorage(url, output_dir):
  """Fetches a tarball from GCS and uncompresses it to |output_dir|."""

  # Pass the compressed stream directly to 'tarfile'; don't bother writing it
  # to disk first.
  cmd = [
      sys.executable,
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'cp', url,
      '-'
  ]

  logging.debug('Running "%s"', ' '.join(cmd))
  task = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
  tar_data = task.stdout
  task.stdout = None  # don't want Popen.communicate() to eat the output

  try:
    tarfile.open(mode='r|gz', fileobj=tar_data).extractall(path=output_dir)
  except tarfile.ReadError as exc:
    _, stderr_data = task.communicate()
    stderr_data = stderr_data.decode()
    raise subprocess.CalledProcessError(
        task.returncode, cmd,
        'Failed to read a tarfile from gsutil.py.\n{}'.format(
            stderr_data if stderr_data else '')) from exc

  if task.wait():
    _, stderr_data = task.communicate()
    stderr_data = stderr_data.decode()
    raise subprocess.CalledProcessError(task.returncode, cmd, stderr_data)

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tarfile
import tempfile

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             'test')))
from gs_util_wrapper import run_gsutil

def DownloadAndUnpackFromCloudStorage(url, output_dir):
  """Fetches a tarball from GCS and uncompresses it to |output_dir|."""

  tmp_file = 'image.tgz'
  with tempfile.TemporaryDirectory() as tmp_d:
    tmp_file_location = os.path.join(tmp_d, tmp_file)
    run_gsutil(['cp', url, tmp_file_location])
    tarfile.open(name=tmp_file_location,
                 mode='r|gz').extractall(path=output_dir)

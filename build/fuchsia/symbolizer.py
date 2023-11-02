# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess

from common import SDK_ROOT
from common import GetHostArchFromPlatform
from common import GetHostToolPathFromPlatform


def BuildIdsPaths(package_paths):
  """Generates build ids paths for symbolizer processes."""

  return [
      os.path.join(os.path.dirname(package_path), 'ids.txt')
      for package_path in package_paths
  ]


def RunSymbolizer(input_fd, output_fd, ids_txt_paths):
  """Starts a symbolizer process.

  input_fd: Input file to be symbolized.
  output_fd: Output file for symbolizer stdout and stderr.
  ids_txt_paths: Path to the ids.txt files which map build IDs to
                 unstripped binaries on the filesystem.
  Returns a Popen object for the started process."""

  symbolizer = GetHostToolPathFromPlatform('symbolizer')
  symbolizer_cmd = [
      symbolizer, '--omit-module-lines', '--build-id-dir',
      os.path.join(SDK_ROOT, '.build-id')
  ]
  for ids_txt in ids_txt_paths:
    symbolizer_cmd.extend(['--ids-txt', ids_txt])

  logging.debug('Running "%s".' % ' '.join(symbolizer_cmd))
  return subprocess.Popen(symbolizer_cmd,
                          stdin=input_fd,
                          stdout=output_fd,
                          stderr=subprocess.STDOUT,
                          close_fds=True)

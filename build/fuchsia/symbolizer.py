# Copyright 2018 The Chromium Authors. All rights reserved.
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

  build_ids_paths = map(
      lambda package_path: os.path.join(
          os.path.dirname(package_path), 'ids.txt'),
      package_paths)
  return build_ids_paths


def RunSymbolizer(input_file, output_file, build_ids_files):
  """Starts a symbolizer process.

  input_file: Input file to be symbolized.
  output_file: Output file for symbolizer stdout and stderr.
  build_ids_file: Path to the ids.txt file which maps build IDs to
                  unstripped binaries on the filesystem.
  Returns a Popen object for the started process."""

  symbolizer = GetHostToolPathFromPlatform('symbolizer')
  symbolizer_cmd = [
      symbolizer, '--build-id-dir',
      os.path.join(SDK_ROOT, '.build-id')
  ]
  for build_ids_file in build_ids_files:
    symbolizer_cmd.extend(['--ids-txt', build_ids_file])

  logging.info('Running "%s".' % ' '.join(symbolizer_cmd))
  return subprocess.Popen(symbolizer_cmd, stdin=input_file, stdout=output_file,
                          stderr=subprocess.STDOUT, close_fds=True)


def SymbolizerFilter(input_file, build_ids_files):
  """Symbolizes an output stream from a process.

  input_file: Input file to be symbolized.
  build_ids_file: Path to the ids.txt file which maps build IDs to
                  unstripped binaries on the filesystem.
  Returns a generator that yields symbolized process output."""

  symbolizer_proc = RunSymbolizer(input_file, subprocess.PIPE, build_ids_files)

  while True:
    # TODO(chonggu): Switch to encoding='utf-8' once we drop Python 2
    # support.
    line = symbolizer_proc.stdout.readline().decode('utf-8')
    if not line:
      break

    # Skip spam emitted by the symbolizer that obscures the symbolized output.
    # TODO(https://crbug.com/1069446): Fix the symbolizer and remove this.
    if '[[[ELF ' in line:
      continue

    yield line

  symbolizer_proc.wait()

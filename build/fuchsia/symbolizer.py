# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess

from common import SDK_ROOT
from common import GetHostArchFromPlatform
from common import GetHostToolPathFromPlatform

# TODO(steveroe): Change 'llvm-3.8' to 'llvm' after docker image is updated.
ARM64_DOCKER_LLVM_SYMBOLIZER_PATH = os.path.join('/', 'usr', 'lib', 'llvm-3.8',
                                                 'bin', 'llvm-symbolizer')

def RunSymbolizer(input_pipe, build_ids_files):
  """Starts a symbolizer process.

  input_pipe: Input pipe to be symbolized.
  build_ids_file: Path to the ids.txt file which maps build IDs to
                  unstripped binaries on the filesystem.
  Returns a Popen object for the started process."""

  if (GetHostArchFromPlatform() == 'arm64' and
      os.path.isfile(ARM64_DOCKER_LLVM_SYMBOLIZER_PATH)):
    llvm_symbolizer_path = ARM64_DOCKER_LLVM_SYMBOLIZER_PATH
  else:
    llvm_symbolizer_path = os.path.join(SDK_ROOT, os.pardir, os.pardir,
                                        'llvm-build', 'Release+Asserts', 'bin',
                                        'llvm-symbolizer')

  symbolizer = GetHostToolPathFromPlatform('symbolize')
  symbolizer_cmd = [symbolizer,
                    '-ids-rel', '-llvm-symbolizer', llvm_symbolizer_path,
                    '-build-id-dir', os.path.join(SDK_ROOT, '.build-id')]
  for build_ids_file in build_ids_files:
    symbolizer_cmd.extend(['-ids', build_ids_file])

  logging.info('Running "%s".' % ' '.join(symbolizer_cmd))
  return subprocess.Popen(symbolizer_cmd, stdout=subprocess.PIPE,
                          stdin=input_pipe, close_fds=True)


def SymbolizerFilter(input_pipe, build_ids_files):
  """Symbolizes an output stream from a process.

  input_pipe: Input pipe to be symbolized.
  build_ids_file: Path to the ids.txt file which maps build IDs to
                  unstripped binaries on the filesystem.
  Returns a generator that yields symbolized process output."""

  symbolizer_proc = RunSymbolizer(input_pipe, build_ids_files)

  while True:
    line = symbolizer_proc.stdout.readline()
    if not line:
      break
    yield line

  symbolizer_proc.wait()

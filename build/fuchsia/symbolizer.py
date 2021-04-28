# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess

from common import SDK_ROOT
from common import GetHostArchFromPlatform
from common import GetHostToolPathFromPlatform

# Paths to the llvm-symbolizer executable in different test hosts.
X64_LLVM_SYMBOLIZER_PATH = os.path.join(SDK_ROOT, os.pardir, os.pardir,
                                        'llvm-build', 'Release+Asserts', 'bin',
                                        'llvm-symbolizer')
ARM64_XENIAL_LLVM_SYMBOLIZER_PATH = os.path.join('/', 'usr', 'lib', 'llvm-3.8',
                                                 'bin', 'llvm-symbolizer')
ARM64_BIONIC_LLVM_SYMBOLIZER_PATH = os.path.join('/', 'usr', 'lib', 'llvm-6.0',
                                                 'bin', 'llvm-symbolizer')


def _GetLLVMSymbolizerPath():
  """Determines the path to the LLVM symbolizer executable based on test host
  architecture and Ubuntu distro."""

  if GetHostArchFromPlatform() == 'x64':
    return X64_LLVM_SYMBOLIZER_PATH

  # Get distro codename from /etc/os-release.
  with open(os.path.join('/', 'etc', 'os-release')) as os_release_file:
    os_release_text = os_release_file.read()
  version_codename_re = r'^VERSION_CODENAME=(?P<codename>[\w.-]+)$'
  match = re.search(version_codename_re, os_release_text, re.MULTILINE)
  codename = match.group('codename') if match else None

  if codename == 'xenial':
    return ARM64_XENIAL_LLVM_SYMBOLIZER_PATH
  elif codename == 'bionic':
    return ARM64_BIONIC_LLVM_SYMBOLIZER_PATH
  else:
    raise Exception('Unknown Ubuntu release "%s"' % codename)


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

  symbolizer = GetHostToolPathFromPlatform('symbolize')
  llvm_symbolizer_path = _GetLLVMSymbolizerPath()
  symbolizer_cmd = [symbolizer,
                    '-ids-rel', '-llvm-symbolizer', llvm_symbolizer_path,
                    '-build-id-dir', os.path.join(SDK_ROOT, '.build-id')]
  for build_ids_file in build_ids_files:
    symbolizer_cmd.extend(['-ids', build_ids_file])

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
    line = symbolizer_proc.stdout.readline()
    if not line:
      break

    # Skip spam emitted by the symbolizer that obscures the symbolized output.
    # TODO(https://crbug.com/1069446): Fix the symbolizer and remove this.
    if '[[[ELF ' in line:
      continue

    yield line

  symbolizer_proc.wait()

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import shutil
import tempfile

TARGET_CPU_MAPPING = {
    'x64': 'x86_64',
    'arm64': 'arm64',
}

METADATA_FILES = ('extract.actionsdata', 'version.json')


def read_json(path):
  """Reads JSON file at `path`."""
  with open(path, encoding='utf8') as stream:
    return json.load(stream)


def add_argument(parser, name, help, required=True):
  """Add argument --{name} to `parser` with description `help`."""
  parser.add_argument(f'--{name}', required=required, help=help)


def extract_metadata(parsed, module_name, swift_files, const_files):
  """
  Extracts metadata for `module_name` according to `parsed`.

  If the extraction fails or no metadata is generated, terminate the script
  with an error (after printing the command stdout/stderr to stderr).
  """

  metadata_dir = os.path.join(parsed.output, 'Metadata.appintents')
  if os.path.exists(metadata_dir):
    shutil.rmtree(metadata_dir)

  target_cpu = TARGET_CPU_MAPPING[parsed.target_cpu]
  target_triple = f'{target_cpu}-apple-ios{parsed.deployment_target}'
  if parsed.target_environment == 'simulator':
    target_triple += '-simulator'

  command = [
      os.path.join(parsed.toolchain_dir, 'usr/bin/appintentsmetadataprocessor'),
      '--toolchain-dir',
      parsed.toolchain_dir,
      '--sdk-root',
      parsed.sdk_root,
      '--deployment-target',
      parsed.deployment_target,
      '--target-triple',
      target_triple,
      '--module-name',
      module_name,
      '--output',
      parsed.output,
      '--binary-file',
      parsed.binary_file,
      '--compile-time-extraction',
  ]

  inputs = set()
  inputs.add(parsed.binary_file)

  for swift_file in swift_files:
    inputs.add(swift_file)
    command.extend(('--source-files', swift_file))

  for const_file in const_files:
    inputs.add(const_file)
    command.extend(('--swift-const-vals', const_file))

  if parsed.xcode_version is not None:
    command.extend(('--xcode-version', parsed.xcode_version))

  process = subprocess.Popen(command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  (stdout, stderr) = process.communicate()

  if process.returncode:
    sys.stderr.write(stdout.decode('utf8'))
    sys.stderr.write(stderr.decode('utf8'))
    return process.returncode

  # Force failure if the tool extracted no data. This is because gn does
  # not support optional outputs and thus it would consider the build as
  # dirty if the output is missing.
  if not os.path.exists(metadata_dir):
    sys.stderr.write(f'error: no metadata generated for {module_name}\n')
    sys.stderr.write(stdout.decode('utf8'))
    sys.stderr.write(stderr.decode('utf8'))
    return 1  # failure

  output_files = METADATA_FILES
  with open(parsed.depfile, 'w', encoding='utf8') as depfile:
    for output in output_files:
      depfile.write(f'{metadata_dir}/{output}:')
      for item in sorted(inputs):
        depfile.write(f' {item}')
      depfile.write('\n')

  return 0  # success


def main(args):
  parser = argparse.ArgumentParser()

  add_argument(parser, 'output', 'path to the output directory')
  add_argument(parser, 'depfile', 'path to the output depfile')
  add_argument(parser, 'toolchain-dir', 'path to the toolchain directory')
  add_argument(parser, 'sdk-root', 'path to the SDK root directory')
  add_argument(parser, 'target-cpu', 'target cpu architecture')
  add_argument(parser, 'target-environment', 'target environment')
  add_argument(parser, 'deployment-target', 'deployment target version')
  add_argument(parser, 'binary-file', 'path to the binary to process')
  add_argument(parser, 'module-info-path', 'path to the module info JSON file')
  add_argument(parser, 'xcode-version', 'version of Xcode', required=False)

  parsed = parser.parse_args(args)

  module_info = read_json(parsed.module_info_path)
  return extract_metadata(
      parsed,  #
      module_info['module_name'],
      module_info['swift_files'],
      module_info['const_files'])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate java source files from protobuf files.

This is the action script for the proto_java_library template.

It performs the following steps:
1. Deletes all old sources (ensures deleted classes are not part of new jars).
2. Creates source directory.
3. Generates Java files using protoc (output into either --java-out-dir or
   --srcjar).
4. Creates a new stamp file.
"""


import argparse
import os
import shutil
import subprocess
import sys

import action_helpers
import zip_helpers

sys.path.append(os.path.join(os.path.dirname(__file__), 'android', 'gyp'))
from util import build_utils


def _HasJavaPackage(proto_lines):
  return any(line.strip().startswith('option java_package')
             for line in proto_lines)


def _EnforceJavaPackage(proto_srcs):
  for proto_path in proto_srcs:
    with open(proto_path) as in_proto:
      if not _HasJavaPackage(in_proto.readlines()):
        raise Exception('Proto files for java must contain a "java_package" '
                        'line: {}'.format(proto_path))


def main(argv):
  parser = argparse.ArgumentParser()
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--protoc', required=True, help='Path to protoc binary.')
  parser.add_argument('--plugin', help='Path to plugin executable')
  parser.add_argument('--proto-path',
                      required=True,
                      help='Path to proto directory.')
  parser.add_argument('--java-out-dir',
                      help='Path to output directory for java files.')
  parser.add_argument('--srcjar', help='Path to output srcjar.')
  parser.add_argument('--stamp', help='File to touch on success.')
  parser.add_argument(
      '--import-dir',
      action='append',
      default=[],
      help='Extra import directory for protos, can be repeated.')
  parser.add_argument('protos', nargs='+', help='proto source files')
  options = parser.parse_args(argv)

  if not options.java_out_dir and not options.srcjar:
    raise Exception('One of --java-out-dir or --srcjar must be specified.')

  _EnforceJavaPackage(options.protos)

  with build_utils.TempDir() as temp_dir:
    protoc_args = []

    generator = 'java'
    if options.plugin:
      generator = 'plugin'
      protoc_args += ['--plugin', 'protoc-gen-plugin=' + options.plugin]

    protoc_args += ['--proto_path', options.proto_path]
    for path in options.import_dir:
      protoc_args += ['--proto_path', path]

    protoc_args += ['--' + generator + '_out=lite:' + temp_dir]

    # Generate Java files using protoc.
    build_utils.CheckOutput(
        [options.protoc] + protoc_args + options.protos,
        # protoc generates superfluous warnings about LITE_RUNTIME deprecation
        # even though we are using the new non-deprecated method.
        stderr_filter=lambda output: build_utils.FilterLines(
            output, '|'.join([r'optimize_for = LITE_RUNTIME', r'java/lite\.md'])
        ))

    if options.java_out_dir:
      build_utils.DeleteDirectory(options.java_out_dir)
      shutil.copytree(temp_dir, options.java_out_dir)
    else:
      with action_helpers.atomic_output(options.srcjar) as f:
        zip_helpers.zip_directory(f, temp_dir)

  if options.depfile:
    assert options.srcjar
    deps = options.protos + [options.protoc]
    action_helpers.write_depfile(options.depfile, options.srcjar, deps)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

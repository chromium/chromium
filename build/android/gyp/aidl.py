#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Invokes Android's aidl
"""

import optparse
import os
import re
import sys
import shutil
import zipfile

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def do_native(options, files):

  for i, f in enumerate(files):
    f = files[i]
    with build_utils.TempDir() as temp_dir:
      aidl_cmd = [options.aidl_path, '--lang=ndk']
      aidl_cmd += [
          '-p' + s for s in action_helpers.parse_gn_list(options.imports)
      ]
      aidl_cmd += ['-I' + s for s in options.includes]
      aidl_cmd += ['-h', options.header_output_dir, '-o', temp_dir]
      aidl_cmd += [f]
      build_utils.CheckOutput(aidl_cmd)

      found_outputs = build_utils.FindInDirectory(temp_dir, '*.cpp')
      assert len(found_outputs) == 1, '\n'.join(found_outputs)
      shutil.move(found_outputs[0], options.cpp_output[i])


def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('--aidl-path', help='Path to the aidl binary.')
  option_parser.add_option('--imports', help='Files to import.')
  option_parser.add_option('--header-output-dir',
                           help='Optional header file output location.')
  option_parser.add_option('--cpp-output',
                           help='Optional cpp file output location.',
                           action='append')
  option_parser.add_option('--includes',
                           help='Directories to add as import search paths.')
  option_parser.add_option('--srcjar', help='Path for srcjar output.')
  action_helpers.add_depfile_arg(option_parser)
  options, args = option_parser.parse_args(argv[1:])

  options.includes = action_helpers.parse_gn_list(options.includes)

  if options.header_output_dir or options.cpp_output:
    if not (options.header_output_dir and options.cpp_output):
      option_parser.error(
          'Native generation requires header-output-dir and cpp-output')


  with build_utils.TempDir() as temp_dir:
    for f in args:
      classname = os.path.splitext(os.path.basename(f))[0]
      output = os.path.join(temp_dir, classname + '.java')
      aidl_cmd = [options.aidl_path]
      aidl_cmd += [
          '-p' + s for s in action_helpers.parse_gn_list(options.imports)
      ]
      aidl_cmd += ['-I' + s for s in options.includes]
      aidl_cmd += [
        f,
        output
      ]
      build_utils.CheckOutput(aidl_cmd)

    with action_helpers.atomic_output(options.srcjar) as f:
      with zipfile.ZipFile(f, 'w') as srcjar:
        for path in build_utils.FindInDirectory(temp_dir, '*.java'):
          with open(path) as fileobj:
            data = fileobj.read()
          pkg_name = re.search(r'^\s*package\s+(.*?)\s*;', data, re.M).group(1)
          arcname = '%s/%s' % (
              pkg_name.replace('.', '/'), os.path.basename(path))
          zip_helpers.add_to_zip_hermetic(srcjar, arcname, data=data)

  if options.header_output_dir:
    do_native(options, args)

  if options.depfile:
    include_files = []
    for include_dir in options.includes:
      include_files += build_utils.FindInDirectory(include_dir, '*.java')
    action_helpers.write_depfile(options.depfile, options.srcjar, include_files)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

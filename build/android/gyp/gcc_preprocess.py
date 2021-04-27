#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import posixpath
import re
import sys
import zipfile

from util import build_utils


def _ParsePackageName(data):
  m = re.match(r'^\s*package\s+(.*?)\s*;', data, re.MULTILINE)
  return m.group(1) if m else ''


def main(args):
  args = build_utils.ExpandFileArgs(args)

  parser = argparse.ArgumentParser()
  parser.add_argument('--include-dirs', help='GN list of include directories.')
  parser.add_argument('--output', help='Path for .srcjar.')
  parser.add_argument('--define',
                      action='append',
                      dest='defines',
                      help='List of -D args')
  parser.add_argument('templates', nargs='+', help='Template files.')
  options = parser.parse_args(args)

  options.defines = build_utils.ParseGnList(options.defines)
  options.include_dirs = build_utils.ParseGnList(options.include_dirs)

  gcc_cmd = [
      'gcc',
      '-E',  # stop after preprocessing.
      '-DANDROID',  # Specify ANDROID define for pre-processor.
      '-x',
      'c-header',  # treat sources as C header files
      '-P',  # disable line markers, i.e. '#line 309'
  ]
  gcc_cmd.extend('-D' + x for x in options.defines)
  gcc_cmd.extend('-I' + x for x in options.include_dirs)

  with build_utils.AtomicOutput(options.output) as f:
    with zipfile.ZipFile(f, 'w') as z:
      for template in options.templates:
        data = build_utils.CheckOutput(gcc_cmd + [template])
        package_name = _ParsePackageName(data)
        if not package_name:
          raise Exception('Could not find java package of ' + template)
        zip_path = posixpath.join(
            package_name.replace('.', '/'),
            os.path.splitext(os.path.basename(template))[0]) + '.java'
        build_utils.AddToZipHermetic(z, zip_path, data=data)


if __name__ == '__main__':
  main(sys.argv[1:])

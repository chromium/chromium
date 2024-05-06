#!/usr/bin/env python3
#
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import posixpath
import re
import sys
import zipfile

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers

_CHROMIUM_SRC = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir)
_LLVM_CLANG_PATH = os.path.join(_CHROMIUM_SRC, 'third_party', 'llvm-build',
                                'Release+Asserts', 'bin', 'clang')

def _ParsePackageName(data):
  m = re.search(r'^\s*package\s+(.*?)\s*;', data, re.MULTILINE)
  return m.group(1) if m else ''


def ProcessJavaFile(template, defines, include_dirs):
  clang_cmd = [
      _LLVM_CLANG_PATH,
      '-E',  # stop after preprocessing.
      '-CC',  # Keep comments
      '-DANDROID',  # Specify ANDROID define for pre-processor.
      '-x',
      'c-header',  # treat sources as C header files
      '-P',  # disable line markers, i.e. '#line 309'
  ]
  clang_cmd.extend('-D' + x for x in defines)
  clang_cmd.extend('-I' + x for x in include_dirs)
  data = build_utils.CheckOutput(clang_cmd + [template])
  package_name = _ParsePackageName(data)
  if not package_name:
    raise Exception('Could not find java package of ' + template)
  return package_name, data


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

  options.defines = action_helpers.parse_gn_list(options.defines)
  options.include_dirs = action_helpers.parse_gn_list(options.include_dirs)
  with action_helpers.atomic_output(options.output) as f:
    with zipfile.ZipFile(f, 'w') as z:
      for template in options.templates:
        package_name, data = ProcessJavaFile(template, options.defines,
                                             options.include_dirs)
        zip_path = posixpath.join(
            package_name.replace('.', '/'),
            os.path.splitext(os.path.basename(template))[0]) + '.java'
        zip_helpers.add_to_zip_hermetic(z, zip_path, data=data)


if __name__ == '__main__':
  main(sys.argv[1:])

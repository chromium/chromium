#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes list of native libraries to srcjar file."""

import argparse
import os
import sys
import zipfile

from native_libraries_template import NATIVE_LIBRARIES_TEMPLATE
from util import build_utils


def main():
  parser = argparse.ArgumentParser()

  parser.add_argument('--final', action='store_true', help='Use final fields.')
  parser.add_argument(
      '--enable-chromium-linker',
      action='store_true',
      help='Enable Chromium linker.')
  parser.add_argument(
      '--load-library-from-apk',
      action='store_true',
      help='Load libaries from APK without uncompressing.')
  parser.add_argument(
      '--enable-chromium-linker-tests', action='store_true', help='Run tests.')
  parser.add_argument(
      '--use-modern-linker', action='store_true', help='To use ModernLinker.')
  parser.add_argument(
      '--native-libraries-list', help='File with list of native libraries.')
  parser.add_argument(
      '--version-number',
      default='""',
      help='Expected version of main library.')
  parser.add_argument(
      '--cpu-family',
      choices={
          'CPU_FAMILY_ARM', 'CPU_FAMILY_X86', 'CPU_FAMILY_MIPS',
          'CPU_FAMILY_UNKNOWN'
      },
      required=True,
      default='CPU_FAMILY_UNKNOWN',
      help='CPU family.')

  parser.add_argument(
      '--output', required=True, help='Path to the generated srcjar file.')

  options = parser.parse_args(build_utils.ExpandFileArgs(sys.argv[1:]))

  assert (options.enable_chromium_linker
          or not options.load_library_from_apk), (
              'Must set --enable-chromium-linker to load library from APK.')

  native_libraries_list = []
  if options.native_libraries_list:
    with open(options.native_libraries_list) as f:
      for path in f:
        path = path.strip()
        filename = os.path.split(path)[1]
        assert filename.startswith('lib')
        assert filename.endswith('.so')
        # Remove lib prefix and .so suffix.
        native_libraries_list.append('"%s"' % filename[3:-3])

  def bool_str(value):
    if value:
      return ' = true'
    elif options.final:
      return ' = false'
    return ''

  format_dict = {
      'MAYBE_FINAL': 'final ' if options.final else '',
      'USE_LINKER': bool_str(options.enable_chromium_linker),
      'USE_LIBRARY_IN_ZIP_FILE': bool_str(options.load_library_from_apk),
      'ENABLE_LINKER_TESTS': bool_str(options.enable_chromium_linker_tests),
      'USE_MODERN_LINKER': bool_str(options.use_modern_linker),
      'LIBRARIES': ','.join(native_libraries_list),
      'VERSION_NUMBER': options.version_number,
      'CPU_FAMILY': options.cpu_family,
  }
  with build_utils.AtomicOutput(options.output) as f:
    with zipfile.ZipFile(f.name, 'w') as srcjar_file:
      build_utils.AddToZipHermetic(
          zip_file=srcjar_file,
          zip_path='org/chromium/base/library_loader/NativeLibraries.java',
          data=NATIVE_LIBRARIES_TEMPLATE.format(**format_dict))


if __name__ == '__main__':
  sys.exit(main())

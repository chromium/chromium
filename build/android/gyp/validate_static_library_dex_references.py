#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from pylib.dex import dex_parser
from util import build_utils

_FLAGS_PATH = (
    '//chrome/android/java/static_library_dex_reference_workarounds.flags')


def _FindIllegalStaticLibraryReferences(static_lib_dex_files,
                                        main_apk_dex_files):
  main_apk_defined_types = set()
  for dex_file in main_apk_dex_files:
    for class_def_item in dex_file.class_def_item_list:
      main_apk_defined_types.add(
          dex_file.GetTypeString(class_def_item.class_idx))

  static_lib_referenced_types = set()
  for dex_file in static_lib_dex_files:
    for type_item in dex_file.type_item_list:
      static_lib_referenced_types.add(
          dex_file.GetString(type_item.descriptor_idx))

  return main_apk_defined_types.intersection(static_lib_referenced_types)


def _DexFilesFromPath(path):
  if zipfile.is_zipfile(path):
    with zipfile.ZipFile(path) as z:
      return [
          dex_parser.DexFile(bytearray(z.read(name))) for name in z.namelist()
          if re.match(r'.*classes[0-9]*\.dex$', name)
      ]
  else:
    with open(path) as f:
      return dex_parser.DexFile(bytearray(f.read()))


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--depfile', required=True, help='Path to output depfile.')
  parser.add_argument(
      '--stamp', required=True, help='Path to file to touch upon success.')
  parser.add_argument(
      '--static-library-dex',
      required=True,
      help='classes.dex or classes.zip for the static library APK that was '
      'proguarded with other dependent APKs')
  parser.add_argument(
      '--static-library-dependent-dex',
      required=True,
      action='append',
      dest='static_library_dependent_dexes',
      help='classes.dex or classes.zip for the APKs that use the static '
      'library APK')
  args = parser.parse_args(args)

  static_library_dexfiles = _DexFilesFromPath(args.static_library_dex)
  for path in args.static_library_dependent_dexes:
    dependent_dexfiles = _DexFilesFromPath(path)
    illegal_references = _FindIllegalStaticLibraryReferences(
        static_library_dexfiles, dependent_dexfiles)

    if illegal_references:
      msg = 'Found illegal references from {} to {}\n'.format(
          args.static_library_dex, path)
      msg += 'Add a -keep rule to avoid this. '
      msg += 'See {} for an example and why this is necessary.\n'.format(
          _FLAGS_PATH)
      msg += 'The illegal references are:\n'
      msg += '\n'.join(illegal_references)
      sys.stderr.write(msg)
      sys.exit(1)

  input_paths = [args.static_library_dex] + args.static_library_dependent_dexes
  build_utils.Touch(args.stamp)
  build_utils.WriteDepfile(args.depfile, args.stamp, inputs=input_paths)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

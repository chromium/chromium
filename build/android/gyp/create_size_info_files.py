#!/usr/bin/env python3

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates size-info/*.info files used by SuperSize."""

import argparse
import collections
import os
import re
import sys
import zipfile

from util import build_utils
from util import jar_info_utils
import action_helpers  # build_utils adds //build to sys.path.


_AAR_VERSION_PATTERN = re.compile(r'/[^/]*?(\.aar/|\.jar/)')


def _RemoveDuplicatesFromList(source_list):
  return collections.OrderedDict.fromkeys(source_list).keys()


def _TransformAarPaths(path):
  # .aar files within //third_party/android_deps have a version suffix.
  # The suffix changes each time .aar files are updated, which makes size diffs
  # hard to compare (since the before/after have different source paths).
  # Rather than changing how android_deps works, we employ this work-around
  # to normalize the paths.
  # From: .../androidx_appcompat_appcompat/appcompat-1.1.0.aar/res/...
  #   To: .../androidx_appcompat_appcompat.aar/res/...
  # https://crbug.com/1056455
  if 'android_deps' not in path:
    return path
  return _AAR_VERSION_PATTERN.sub(r'\1', path)


def _MergeResInfoFiles(res_info_path, info_paths):
  # Concatenate them all.
  with action_helpers.atomic_output(res_info_path, 'w+') as dst:
    for p in info_paths:
      with open(p) as src:
        dst.writelines(_TransformAarPaths(l) for l in src)


def _PakInfoPathsForAssets(assets):
  # Use "in" rather than "endswith" due to suffix. https://crbug.com/357131361
  return [f.split(':')[0] + '.info' for f in assets if '.pak' in f]


def _MergePakInfoFiles(merged_path, pak_infos):
  info_lines = set()
  for pak_info_path in pak_infos:
    with open(pak_info_path, 'r') as src_info_file:
      info_lines.update(_TransformAarPaths(x) for x in src_info_file)
  # only_if_changed=False since no build rules depend on this as an input.
  with action_helpers.atomic_output(merged_path,
                                    only_if_changed=False,
                                    mode='w+') as f:
    f.writelines(sorted(info_lines))


def _FullJavaNameFromClassFilePath(path):
  # Input:  base/android/java/src/org/chromium/Foo.class
  # Output: base.android.java.src.org.chromium.Foo
  if not path.endswith('.class'):
    return ''
  path = os.path.splitext(path)[0]
  parts = []
  while path:
    # Use split to be platform independent.
    head, tail = os.path.split(path)
    path = head
    parts.append(tail)
  parts.reverse()  # Package comes first
  return '.'.join(parts)


def _MergeJarInfoFiles(output, inputs):
  """Merge several .jar.info files to generate an .apk.jar.info.

  Args:
    output: output file path.
    inputs: List of .jar.info or .jar files.
  """
  info_data = dict()
  for path in inputs:
    # For non-prebuilts: .jar.info files are written by compile_java.py and map
    # .class files to .java source paths.
    #
    # For prebuilts: No .jar.info file exists, we scan the .jar files here and
    # map .class files to the .jar.
    #
    # For .aar files: We look for a "source.info" file in the containing
    # directory in order to map classes back to the .aar (rather than mapping
    # them to the extracted .jar file).
    if path.endswith('.info'):
      info_data.update(jar_info_utils.ParseJarInfoFile(path))
    else:
      attributed_path = path
      if not path.startswith('..'):
        parent_path = os.path.dirname(path)
        # See if it's an sub-jar within the .aar.
        if os.path.basename(parent_path) == 'libs':
          parent_path = os.path.dirname(parent_path)
        aar_source_info_path = os.path.join(parent_path, 'source.info')
        # source.info files exist only for jars from android_aar_prebuilt().
        # E.g. Could have an java_prebuilt() pointing to a generated .jar.
        if os.path.exists(aar_source_info_path):
          attributed_path = jar_info_utils.ReadAarSourceInfo(
              aar_source_info_path)

      with zipfile.ZipFile(path) as zip_info:
        for name in zip_info.namelist():
          fully_qualified_name = _FullJavaNameFromClassFilePath(name)
          if fully_qualified_name:
            info_data[fully_qualified_name] = _TransformAarPaths('{}/{}'.format(
                attributed_path, name))

  # only_if_changed=False since no build rules depend on this as an input.
  with action_helpers.atomic_output(output, only_if_changed=False) as f:
    jar_info_utils.WriteJarInfoFile(f, info_data)


def _FindJarInputs(jar_paths):
  ret = []
  for jar_path in jar_paths:
    jar_info_path = jar_path + '.info'
    if os.path.exists(jar_info_path):
      ret.append(jar_info_path)
    else:
      ret.append(jar_path)
  return ret


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser(description=__doc__)
  action_helpers.add_depfile_arg(parser)
  parser.add_argument(
      '--jar-info-path', required=True, help='Output .jar.info file')
  parser.add_argument(
      '--pak-info-path', required=True, help='Output .pak.info file')
  parser.add_argument(
      '--res-info-path', required=True, help='Output .res.info file')
  parser.add_argument(
      '--jar-files',
      required=True,
      action='append',
      help='GN-list of .jar file paths')
  parser.add_argument(
      '--assets',
      required=True,
      action='append',
      help='GN-list of files to add as assets in the form '
      '"srcPath:zipPath", where ":zipPath" is optional.')
  parser.add_argument(
      '--uncompressed-assets',
      required=True,
      action='append',
      help='Same as --assets, except disables compression.')
  parser.add_argument(
      '--in-res-info-path',
      required=True,
      action='append',
      help='Paths to .ap_.info files')

  options = parser.parse_args(args)

  options.jar_files = action_helpers.parse_gn_list(options.jar_files)
  options.assets = action_helpers.parse_gn_list(options.assets)
  options.uncompressed_assets = action_helpers.parse_gn_list(
      options.uncompressed_assets)

  jar_inputs = _FindJarInputs(_RemoveDuplicatesFromList(options.jar_files))
  pak_inputs = _PakInfoPathsForAssets(options.assets +
                                      options.uncompressed_assets)
  res_inputs = options.in_res_info_path

  # Just create the info files every time. See https://crbug.com/1045024
  _MergeJarInfoFiles(options.jar_info_path, jar_inputs)
  _MergePakInfoFiles(options.pak_info_path, pak_inputs)
  _MergeResInfoFiles(options.res_info_path, res_inputs)

  all_inputs = jar_inputs + pak_inputs + res_inputs
  action_helpers.write_depfile(options.depfile,
                               options.jar_info_path,
                               inputs=all_inputs)


if __name__ == '__main__':
  main(sys.argv[1:])

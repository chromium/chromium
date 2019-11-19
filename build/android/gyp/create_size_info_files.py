#!/usr/bin/env python

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates size-info/*.info files used by SuperSize."""

import argparse
import os
import sys
import zipfile

from util import build_utils
from util import jar_info_utils
from util import md5_check


def _MergeResInfoFiles(res_info_path, info_paths):
  # Concatenate them all.
  # only_if_changed=False since no build rules depend on this as an input.
  with build_utils.AtomicOutput(res_info_path, only_if_changed=False) as dst:
    for p in info_paths:
      with open(p) as src:
        dst.write(src.read())


def _PakInfoPathsForAssets(assets):
  return [f.split(':')[0] + '.info' for f in assets if f.endswith('.pak')]


def _MergePakInfoFiles(merged_path, pak_infos):
  info_lines = set()
  for pak_info_path in pak_infos:
    with open(pak_info_path, 'r') as src_info_file:
      info_lines.update(src_info_file.readlines())
  # only_if_changed=False since no build rules depend on this as an input.
  with build_utils.AtomicOutput(merged_path, only_if_changed=False) as f:
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
    inputs: List of .info.jar or .jar files.
  """
  info_data = dict()
  for path in inputs:
    # android_java_prebuilt adds jar files in the src directory (relative to
    #     the output directory, usually ../../third_party/example.jar).
    # android_aar_prebuilt collects jar files in the aar file and uses the
    #     java_prebuilt rule to generate gen/example/classes.jar files.
    # We scan these prebuilt jars to parse each class path for the FQN. This
    #     allows us to later map these classes back to their respective src
    #     directories.
    # TODO(agrieve): This should probably also check that the mtime of the .info
    #     is newer than that of the .jar, or change prebuilts to always output
    #     .info files so that they always exist (and change the depfile to
    #     depend directly on them).
    if path.endswith('.info'):
      info_data.update(jar_info_utils.ParseJarInfoFile(path))
    else:
      with zipfile.ZipFile(path) as zip_info:
        for name in zip_info.namelist():
          fully_qualified_name = _FullJavaNameFromClassFilePath(name)
          if fully_qualified_name:
            info_data[fully_qualified_name] = '{}/{}'.format(path, name)

  # only_if_changed=False since no build rules depend on this as an input.
  with build_utils.AtomicOutput(output, only_if_changed=False) as f:
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
  build_utils.AddDepfileOption(parser)
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

  options.jar_files = build_utils.ParseGnList(options.jar_files)
  options.assets = build_utils.ParseGnList(options.assets)
  options.uncompressed_assets = build_utils.ParseGnList(
      options.uncompressed_assets)

  jar_inputs = _FindJarInputs(set(options.jar_files))
  pak_inputs = _PakInfoPathsForAssets(options.assets +
                                      options.uncompressed_assets)
  res_inputs = options.in_res_info_path

  # Don't bother re-running if no .info files have changed (saves ~250ms).
  md5_check.CallAndRecordIfStale(
      lambda: _MergeJarInfoFiles(options.jar_info_path, jar_inputs),
      input_paths=jar_inputs,
      output_paths=[options.jar_info_path])

  # Always recreate these (just as fast as md5 checking them).
  _MergePakInfoFiles(options.pak_info_path, pak_inputs)
  _MergeResInfoFiles(options.res_info_path, res_inputs)

  all_inputs = jar_inputs + pak_inputs + res_inputs
  build_utils.WriteDepfile(
      options.depfile,
      options.jar_info_path,
      inputs=all_inputs,
      add_pydeps=False)


if __name__ == '__main__':
  main(sys.argv[1:])

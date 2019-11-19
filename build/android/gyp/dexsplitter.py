#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys
import zipfile

from util import build_utils


def _ParseOptions(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--depfile', help='Path to the depfile to write to.')
  parser.add_argument('--stamp', help='Path to stamp to mark when finished.')
  parser.add_argument('--r8-path', help='Path to the r8.jar to use.')
  parser.add_argument(
      '--input-dex-zip', help='Path to dex files in zip being split.')
  parser.add_argument(
      '--proguard-mapping-file', help='Path to proguard mapping file.')
  parser.add_argument(
      '--feature-name',
      action='append',
      dest='feature_names',
      help='The name of the feature module.')
  parser.add_argument(
      '--feature-jars',
      action='append',
      help='GN list of path to jars which compirse the corresponding feature.')
  parser.add_argument(
      '--dex-dest',
      action='append',
      dest='dex_dests',
      help='Destination for dex file of the corresponding feature.')
  options = parser.parse_args(args)

  assert len(options.feature_names) == len(options.feature_jars) and len(
      options.feature_names) == len(options.dex_dests)
  options.features = {}
  for i, name in enumerate(options.feature_names):
    options.features[name] = build_utils.ParseGnList(options.feature_jars[i])

  return options


def _RunDexsplitter(options, output_dir):
  cmd = [
      build_utils.JAVA_PATH,
      '-jar',
      options.r8_path,
      'dexsplitter',
      '--output',
      output_dir,
      '--proguard-map',
      options.proguard_mapping_file,
  ]

  for base_jar in options.features['base']:
    cmd += ['--base-jar', base_jar]

  base_jars_lookup = set(options.features['base'])
  for feature in options.features:
    if feature == 'base':
      continue
    for feature_jar in options.features[feature]:
      if feature_jar not in base_jars_lookup:
        cmd += ['--feature-jar', feature_jar + ':' + feature]

  with build_utils.TempDir() as temp_dir:
    unzipped_files = build_utils.ExtractAll(options.input_dex_zip, temp_dir)
    for file_name in unzipped_files:
      cmd += ['--input', file_name]
    build_utils.CheckOutput(cmd)


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseOptions(args)

  input_paths = [options.input_dex_zip]
  for feature_jars in options.features.itervalues():
    for feature_jar in feature_jars:
      input_paths.append(feature_jar)

  with build_utils.TempDir() as dexsplitter_output_dir:
    curr_location_to_dest = []
    if len(options.features) == 1:
      # Don't run dexsplitter since it needs at least 1 feature module.
      curr_location_to_dest.append((options.input_dex_zip,
                                    options.dex_dests[0]))
    else:
      _RunDexsplitter(options, dexsplitter_output_dir)

      for i, dest in enumerate(options.dex_dests):
        module_dex_file = os.path.join(dexsplitter_output_dir,
                                       options.feature_names[i], 'classes.dex')
        if os.path.exists(module_dex_file):
          curr_location_to_dest.append((module_dex_file, dest))
        else:
          module_dex_file += '.zip'
          assert os.path.exists(
              module_dex_file), 'Dexsplitter tool output not found.'
          curr_location_to_dest.append((module_dex_file + '.zip', dest))

    for curr_location, dest in curr_location_to_dest:
      with build_utils.AtomicOutput(dest) as f:
        if curr_location.endswith('.zip'):
          if dest.endswith('.zip'):
            shutil.copy(curr_location, f.name)
          else:
            with zipfile.ZipFile(curr_location, 'r') as z:
              namelist = z.namelist()
              assert len(namelist) == 1, (
                  'Unzipping to single dex file, but not single dex file in ' +
                  options.input_dex_zip)
              z.extract(namelist[0], f.name)
        else:
          if dest.endswith('.zip'):
            build_utils.ZipDir(
                f.name, os.path.abspath(os.path.join(curr_location, os.pardir)))
          else:
            shutil.move(curr_location, f.name)

  build_utils.Touch(options.stamp)
  build_utils.WriteDepfile(options.depfile, options.stamp, inputs=input_paths)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

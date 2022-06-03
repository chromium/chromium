#!/usr/bin/env python3
# encoding: utf-8
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
from util import build_utils
from util import resource_utils


def main(args):
  parser = argparse.ArgumentParser()

  build_utils.AddDepfileOption(parser)
  parser.add_argument('--script',
                      required=True,
                      help='Path to the unused resources detector script.')
  parser.add_argument(
      '--dependencies-res-zips',
      required=True,
      action='append',
      help='Resources zip archives to investigate for unused resources.')
  parser.add_argument('--dexes',
                      action='append',
                      required=True,
                      help='Path to dex file, or zip with dex files.')
  parser.add_argument(
      '--proguard-mapping',
      help='Path to proguard mapping file for the optimized dex.')
  parser.add_argument('--r-text', required=True, help='Path to R.txt')
  parser.add_argument('--android-manifests',
                      action='append',
                      required=True,
                      help='Path to AndroidManifest')
  parser.add_argument('--output-config',
                      required=True,
                      help='Path to output the aapt2 config to.')
  args = build_utils.ExpandFileArgs(args)
  options = parser.parse_args(args)
  options.dependencies_res_zips = (build_utils.ParseGnList(
      options.dependencies_res_zips))

  # in case of no resources, short circuit early.
  if not options.dependencies_res_zips:
    build_utils.Touch(options.output_config)
    return

  with build_utils.TempDir() as temp_dir:
    dep_subdirs = []
    for dependency_res_zip in options.dependencies_res_zips:
      dep_subdirs += resource_utils.ExtractDeps([dependency_res_zip], temp_dir)

    cmd = [
        options.script,
        '--rtxts',
        options.r_text,
        '--manifests',
        ':'.join(options.android_manifests),
        '--resourceDirs',
        ':'.join(dep_subdirs),
        '--dexes',
        ':'.join(options.dexes),
        '--outputConfig',
        options.output_config,
    ]
    if options.proguard_mapping:
      cmd += [
          '--mapping',
          options.proguard_mapping,
      ]
    build_utils.CheckOutput(cmd)

  if options.depfile:
    depfile_deps = (options.dependencies_res_zips + options.android_manifests +
                    options.dexes) + [options.r_text]
    if options.proguard_mapping:
      depfile_deps.append(options.proguard_mapping)
    build_utils.WriteDepfile(options.depfile, options.output_config,
                             depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])

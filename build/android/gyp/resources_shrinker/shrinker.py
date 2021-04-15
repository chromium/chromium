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
      help='Resources zip archives to investigate for unused resources.')
  parser.add_argument('--dex',
                      required=True,
                      help='Path to dex file, or zip with dex files.')
  parser.add_argument(
      '--proguard-mapping',
      required=True,
      help='Path to proguard mapping file for the optimized dex.')
  parser.add_argument('--r-text', required=True, help='Path to R.txt')
  parser.add_argument('--android-manifest',
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

    build_utils.CheckOutput([
        options.script, '--rtxts', options.r_text, '--manifests',
        options.android_manifest, '--resourceDirs', ':'.join(dep_subdirs),
        '--dex', options.dex, '--mapping', options.proguard_mapping,
        '--outputConfig', options.output_config
    ])

  if options.depfile:
    depfile_deps = options.dependencies_res_zips + [
        options.r_text,
        options.android_manifest,
        options.dex,
        options.proguard_mapping,
    ]
    build_utils.WriteDepfile(options.depfile, options.output_config,
                             depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])

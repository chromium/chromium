#!/usr/bin/env python3
# encoding: utf-8
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pathlib
import sys

from util import build_utils
from util import resource_utils
import action_helpers  # build_utils adds //build to sys.path.


def _FilterUnusedResources(r_text_in, r_text_out, unused_resources_config):
  removed_resources = set()
  with open(unused_resources_config, encoding='utf-8') as output_config:
    for line in output_config:
      # example line: attr/line_height#remove
      resource = line.split('#')[0]
      resource_type, resource_name = resource.split('/')
      removed_resources.add((resource_type, resource_name))
  kept_lines = []
  with open(r_text_in, encoding='utf-8') as infile:
    for line in infile:
      # example line: int attr line_height 0x7f0014ee
      resource_type, resource_name = line.split(' ')[1:3]
      if (resource_type, resource_name) not in removed_resources:
        kept_lines.append(line)

  with open(r_text_out, 'w', encoding='utf-8') as out_file:
    out_file.writelines(kept_lines)


def _WritePaths(dest_path, lines):
  pathlib.Path(dest_path).write_text('\n'.join(lines) + '\n')


def main(args):
  parser = argparse.ArgumentParser()

  action_helpers.add_depfile_arg(parser)
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
  parser.add_argument('--r-text-in', required=True, help='Path to input R.txt')
  parser.add_argument(
      '--r-text-out',
      help='Path to output R.txt with unused resources removed.')
  parser.add_argument('--android-manifests',
                      action='append',
                      required=True,
                      help='Path to AndroidManifest')
  parser.add_argument('--output-config',
                      required=True,
                      help='Path to output the aapt2 config to.')
  args = build_utils.ExpandFileArgs(args)
  options = parser.parse_args(args)
  options.dependencies_res_zips = (action_helpers.parse_gn_list(
      options.dependencies_res_zips))

  # in case of no resources, short circuit early.
  if not options.dependencies_res_zips:
    build_utils.Touch(options.output_config)
    return

  with build_utils.TempDir() as temp_dir:
    dep_subdirs = []
    for dependency_res_zip in options.dependencies_res_zips:
      dep_subdirs += resource_utils.ExtractDeps([dependency_res_zip], temp_dir)

    # Use files for paths to avoid command line getting too long.
    # https://crbug.com/362019371
    manifests_file = os.path.join(temp_dir, 'manifests-inputs.txt')
    resources_file = os.path.join(temp_dir, 'resources-inputs.txt')
    dexes_file = os.path.join(temp_dir, 'dexes-inputs.txt')

    _WritePaths(manifests_file, options.android_manifests)
    _WritePaths(resources_file, dep_subdirs)
    _WritePaths(dexes_file, options.dexes)

    cmd = [
        options.script,
        '--rtxts',
        options.r_text_in,
        '--manifests',
        manifests_file,
        '--resourceDirs',
        resources_file,
        '--dexes',
        dexes_file,
        '--outputConfig',
        options.output_config,
    ]
    if options.proguard_mapping:
      cmd += [
          '--mapping',
          options.proguard_mapping,
      ]
    build_utils.CheckOutput(cmd)

  if options.r_text_out:
    _FilterUnusedResources(options.r_text_in, options.r_text_out,
                           options.output_config)

  if options.depfile:
    depfile_deps = (options.dependencies_res_zips + options.android_manifests +
                    options.dexes) + [options.r_text_in]
    if options.proguard_mapping:
      depfile_deps.append(options.proguard_mapping)
    action_helpers.write_depfile(options.depfile, options.output_config,
                                 depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])

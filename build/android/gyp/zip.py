#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Archives a set of files."""

import argparse
import json
import os
import sys
import zipfile

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = argparse.ArgumentParser(args)
  parser.add_argument('--input-files', help='GN-list of files to zip.')
  parser.add_argument(
      '--input-files-base-dir',
      help='Paths in the archive will be relative to this directory')
  parser.add_argument('--input-zips', help='GN-list of zips to merge.')
  parser.add_argument(
      '--input-zips-excluded-globs',
      help='GN-list of globs for paths to exclude.')
  parser.add_argument('--output', required=True, help='Path to output archive.')
  compress_group = parser.add_mutually_exclusive_group()
  compress_group.add_argument(
      '--compress', action='store_true', help='Compress entries')
  compress_group.add_argument(
      '--no-compress',
      action='store_false',
      dest='compress',
      help='Do not compress entries')
  parser.add_argument('--comment-json',
                      action='append',
                      metavar='KEY=VALUE',
                      type=lambda x: x.split('=', 1),
                      help='Entry to store in JSON-encoded archive comment.')
  action_helpers.add_depfile_arg(parser)
  options = parser.parse_args(args)

  with action_helpers.atomic_output(options.output) as f:
    with zipfile.ZipFile(f.name, 'w') as out_zip:
      depfile_deps = None
      if options.input_files:
        files = action_helpers.parse_gn_list(options.input_files)
        zip_helpers.add_files_to_zip(files,
                                     out_zip,
                                     base_dir=options.input_files_base_dir,
                                     compress=options.compress)

      if options.input_zips:
        files = action_helpers.parse_gn_list(options.input_zips)
        depfile_deps = files
        path_transform = None
        if options.input_zips_excluded_globs:
          globs = action_helpers.parse_gn_list(
              options.input_zips_excluded_globs)
          path_transform = (
              lambda p: None if build_utils.MatchesGlob(p, globs) else p)
        zip_helpers.merge_zips(out_zip,
                               files,
                               path_transform=path_transform,
                               compress=options.compress)

      if options.comment_json:
        out_zip.comment = json.dumps(dict(options.comment_json),
                                     sort_keys=True).encode('utf-8')

  # Depfile used only by dist_jar().
  if options.depfile:
    action_helpers.write_depfile(options.depfile,
                                 options.output,
                                 inputs=depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])

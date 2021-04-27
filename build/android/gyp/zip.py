#!/usr/bin/env python3
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Archives a set of files."""

import argparse
import os
import sys
import zipfile

from util import build_utils


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
  build_utils.AddDepfileOption(parser)
  options = parser.parse_args(args)

  with build_utils.AtomicOutput(options.output) as f:
    with zipfile.ZipFile(f.name, 'w') as out_zip:
      depfile_deps = None
      if options.input_files:
        files = build_utils.ParseGnList(options.input_files)
        build_utils.DoZip(
            files,
            out_zip,
            base_dir=options.input_files_base_dir,
            compress_fn=lambda _: options.compress)

      if options.input_zips:
        files = build_utils.ParseGnList(options.input_zips)
        depfile_deps = files
        path_transform = None
        if options.input_zips_excluded_globs:
          globs = build_utils.ParseGnList(options.input_zips_excluded_globs)
          path_transform = (
              lambda p: None if build_utils.MatchesGlob(p, globs) else p)
        build_utils.MergeZips(
            out_zip,
            files,
            path_transform=path_transform,
            compress=options.compress)

  # Depfile used only by dist_jar().
  if options.depfile:
    build_utils.WriteDepfile(options.depfile,
                             options.output,
                             inputs=depfile_deps)


if __name__ == '__main__':
  main(sys.argv[1:])

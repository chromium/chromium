#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Writes a dummy R.java file from a list of R.txt files."""

import argparse
import sys

from util import build_utils
from util import resource_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def _ConcatRTxts(rtxt_in_paths, combined_out_path):
  all_lines = set()
  for rtxt_in_path in rtxt_in_paths:
    with open(rtxt_in_path) as rtxt_in:
      all_lines.update(rtxt_in.read().splitlines())
  with open(combined_out_path, 'w') as combined_out:
    combined_out.write('\n'.join(sorted(all_lines)))


def _CreateRJava(rtxts, package_name, srcjar_out):
  with resource_utils.BuildContext() as build:
    _ConcatRTxts(rtxts, build.r_txt_path)
    rjava_build_options = resource_utils.RJavaBuildOptions()
    rjava_build_options.ExportAllResources()
    rjava_build_options.ExportAllStyleables()
    rjava_build_options.GenerateOnResourcesLoaded(fake=True)
    resource_utils.CreateRJavaFiles(build.srcjar_dir,
                                    package_name,
                                    build.r_txt_path,
                                    extra_res_packages=[],
                                    rjava_build_options=rjava_build_options,
                                    srcjar_out=srcjar_out,
                                    ignore_mismatched_values=True)
    with action_helpers.atomic_output(srcjar_out) as f:
      zip_helpers.zip_directory(f, build.srcjar_dir)


def main(args):
  parser = argparse.ArgumentParser(description='Create an R.java srcjar.')
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--srcjar-out',
                      required=True,
                      help='Path to output srcjar.')
  parser.add_argument('--deps-rtxts',
                      required=True,
                      help='List of rtxts of resource dependencies.')
  parser.add_argument('--r-package',
                      required=True,
                      help='R.java package to use.')
  options = parser.parse_args(build_utils.ExpandFileArgs(args))
  options.deps_rtxts = action_helpers.parse_gn_list(options.deps_rtxts)

  _CreateRJava(options.deps_rtxts, options.r_package, options.srcjar_out)
  action_helpers.write_depfile(options.depfile,
                               options.srcjar_out,
                               inputs=options.deps_rtxts)


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))

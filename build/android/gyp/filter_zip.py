#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import shutil
import sys

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def CreatePathTransform(exclude_globs, include_globs):
  """Returns a function to strip paths for the given patterns.

  Args:
    exclude_globs: List of globs that if matched should be excluded.
    include_globs: List of globs that if not matched should be excluded.

  Returns:
    * None if no filters are needed.
    * A function "(path) -> path" that returns None when |path| should be
          stripped, or |path| otherwise.
  """
  if not (exclude_globs or include_globs):
    return None
  exclude_globs = list(exclude_globs or [])
  def path_transform(path):
    # Exclude filters take precidence over include filters.
    if build_utils.MatchesGlob(path, exclude_globs):
      return None
    if include_globs and not build_utils.MatchesGlob(path, include_globs):
      return None
    return path

  return path_transform


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True,
      help='Input zip file.')
  parser.add_argument('--output', required=True,
      help='Output zip file')
  parser.add_argument('--exclude-globs',
      help='GN list of exclude globs')
  parser.add_argument('--include-globs',
      help='GN list of include globs')
  argv = build_utils.ExpandFileArgs(sys.argv[1:])
  args = parser.parse_args(argv)

  args.exclude_globs = action_helpers.parse_gn_list(args.exclude_globs)
  args.include_globs = action_helpers.parse_gn_list(args.include_globs)

  path_transform = CreatePathTransform(args.exclude_globs, args.include_globs)
  with action_helpers.atomic_output(args.output) as f:
    if path_transform:
      zip_helpers.merge_zips(f.name, [args.input],
                             path_transform=path_transform)
    else:
      shutil.copy(args.input, f.name)


if __name__ == '__main__':
  main()

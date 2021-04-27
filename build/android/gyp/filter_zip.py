#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import shutil
import sys

from util import build_utils


_RESOURCE_CLASSES = [
    "R.class",
    "R##*.class",
    "Manifest.class",
    "Manifest##*.class",
]


def CreatePathTransform(exclude_globs, include_globs,
                        strip_resource_classes_for):
  """Returns a function to strip paths for the given patterns.

  Args:
    exclude_globs: List of globs that if matched should be excluded.
    include_globs: List of globs that if not matched should be excluded.
    strip_resource_classes_for: List of Java packages for which to strip
       R.java classes from.

  Returns:
    * None if no filters are needed.
    * A function "(path) -> path" that returns None when |path| should be
          stripped, or |path| otherwise.
  """
  if not (exclude_globs or include_globs or strip_resource_classes_for):
    return None
  exclude_globs = list(exclude_globs or [])
  if strip_resource_classes_for:
    exclude_globs.extend(p.replace('.', '/') + '/' + f
                         for p in strip_resource_classes_for
                         for f in _RESOURCE_CLASSES)
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
  parser.add_argument('--strip-resource-classes-for',
      help='GN list of java package names exclude R.class files in.')

  argv = build_utils.ExpandFileArgs(sys.argv[1:])
  args = parser.parse_args(argv)

  args.exclude_globs = build_utils.ParseGnList(args.exclude_globs)
  args.include_globs = build_utils.ParseGnList(args.include_globs)
  args.strip_resource_classes_for = build_utils.ParseGnList(
      args.strip_resource_classes_for)

  path_transform = CreatePathTransform(args.exclude_globs, args.include_globs,
                                       args.strip_resource_classes_for)
  with build_utils.AtomicOutput(args.output) as f:
    if path_transform:
      build_utils.MergeZips(f.name, [args.input], path_transform=path_transform)
    else:
      shutil.copy(args.input, f.name)


if __name__ == '__main__':
  main()

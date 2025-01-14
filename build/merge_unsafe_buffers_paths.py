#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Combines several unsafe_buffer_paths.txt files into a single file.

Adjusts relative paths for each entry as needed. The first file from the
command line argument list is treated as rooted and its entries are not
adjusted. The entries in the remaining files are adjusted relative to
the path of the file itself.
"""

import optparse
import os
import sys

import action_helpers


def extract_entry(line):
  """Remove comments and whitespace from an unsafe_buffers_paths file entry,
  e.g. ' +foo/  # comment' -> '+foo/'.
  """
  return line.split('#')[0].strip()


def adjust_path(prefix, line):
  """Adjusts a path to be relative to the top-level directory, e.g. in a
  third_party/product file '-notsafe/' -> '-third_party/product/notsafe/'.
  """
  return line[0] + prefix + line[1:]


def strip_dot_dot_slash(line):
  """Removes leading '../' from a path to a file (intended to be outside
  the build directory), e.g. '../../../file.txt' -> 'file.txt'.
  """
  while line.startswith("../"):
    line = line[3:]
  return line


def main(argv):
  parser = optparse.OptionParser()
  usage = 'Usage: %prog <main_input> [<other_input>...] <output>'
  parser.set_usage(usage)
  options, arglist = parser.parse_args(argv)

  if len(arglist) < 3:
    parser.print_help()
    return 1

  main_file = arglist[1]
  other_files = arglist[2:-1]
  output_file = arglist[-1]

  contents = []
  with open(main_file, 'r') as f:
    for line in f:
      line = extract_entry(line)
      if line:
        contents.append(line)

  for other_file in other_files:
    with open(other_file, 'r') as f:
      prefix = strip_dot_dot_slash(os.path.dirname(other_file)) + "/"
      contents.append('+' + prefix)
      for line in f:
        line = extract_entry(line)
        if line:
          contents.append(adjust_path(prefix, line))

  with action_helpers.atomic_output(output_file, mode='w') as f:
    f.write('\n'.join(contents))
    f.write('\n')


if __name__ == '__main__':
  sys.exit(main(sys.argv))

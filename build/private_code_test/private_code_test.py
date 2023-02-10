#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests that no linker inputs are from private paths."""

import argparse
import os
import pathlib
import sys

_DIR_SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--linker-inputs',
                      required=True,
                      help='Path to file containing one linker input per line, '
                      'relative to --root-out-dir')
  parser.add_argument('--private-paths-file',
                      required=True,
                      help='Path to file containing list of paths that are '
                      'considered private, relative gclient root.')
  parser.add_argument('--root-out-dir',
                      required=True,
                      help='See --linker-inputs.')
  parser.add_argument('--expect-failure',
                      action='store_true',
                      help='Invert exit code.')
  args = parser.parse_args()

  private_paths = pathlib.Path(args.private_paths_file).read_text().splitlines()
  linker_inputs = pathlib.Path(args.linker_inputs).read_text().splitlines()

  # Remove src/ prefix from paths.
  # We care only about paths within src/ since GN cannot reference files
  # outside of // (and what would the obj/ path for them look like?).
  private_paths = [p[4:] for p in private_paths if p.startswith('src/')]
  if not private_paths:
    raise ('No paths src/ paths found in ' + args.private_paths_file)

  root_out_dir = args.root_out_dir
  if root_out_dir == '.':
    root_out_dir = ''

  seen = set()
  found = []
  for linker_input in linker_inputs:
    dirname = os.path.dirname(linker_input)
    if dirname in seen:
      continue

    to_check = dirname
    # Strip ../ prefix.
    if to_check.startswith('..'):
      to_check = os.path.relpath(to_check, _DIR_SRC_ROOT)
    else:
      if root_out_dir:
        # Strip secondary toolchain subdir
        to_check = to_check[len(root_out_dir) + 1:]
      # Strip top-level dir (e.g. "obj", "gen").
      parts = to_check.split(os.path.sep, 1)
      if len(parts) == 1:
        continue
      to_check = parts[1]

    if any(to_check.startswith(p) for p in private_paths):
      found.append(linker_input)
    else:
      seen.add(dirname)

  if found:
    limit = 10 if args.expect_failure else 1000
    print('Found private paths being linked into public code:')
    for path in found[:limit]:
      print(f'{path}')
    if len(found) > limit:
      print(f'... and {len(found) - limit} more.')
  elif args.expect_failure:
    print('Expected to find a private path, but none were found.')

  sys.exit(0 if bool(found) == args.expect_failure else 1)


if __name__ == '__main__':
  main()

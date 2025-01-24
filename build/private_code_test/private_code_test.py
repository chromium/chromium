#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests that no linker inputs are from private paths."""

import argparse
import fnmatch
import os
import pathlib
import sys

_DIR_SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]


def _print_paths(paths, limit):
  for path in paths[:limit]:
    print(path)
  if len(paths) > limit:
    print(f'... and {len(paths) - limit} more.')
  print()


def _apply_allowlist(found, globs):
  ignored_paths = []
  new_found = []
  for path in found:
    for pattern in globs:
      if fnmatch.fnmatch(path, pattern):
        ignored_paths.append(path)
        break
    else:
      new_found.append(path)
  return new_found, ignored_paths


def _find_private_paths(linker_inputs, private_paths, root_out_dir):
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
  return found


def _read_private_paths(path):
  text = pathlib.Path(path).read_text()

  # Check if .gclient_entries was not valid.  https://crbug.com/1427829
  if text.startswith('# ERROR: '):
    sys.stderr.write(text)
    sys.exit(1)

  # Remove src/ prefix from paths.
  # We care only about paths within src/ since GN cannot reference files
  # outside of // (and what would the obj/ path for them look like?).
  ret = [p[4:] for p in text.splitlines() if p.startswith('src/')]
  if not ret:
    sys.stderr.write(f'No src/ paths found in {args.private_paths_file}\n')
    sys.stderr.write(f'This test should not be run on public bots.\n')
    sys.stderr.write(f'File contents:\n')
    sys.stderr.write(text)
    sys.exit(1)

  return ret


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
  parser.add_argument('--allow-violation',
                      action='append',
                      help='globs of private paths to allow.')
  parser.add_argument('--expect-failure',
                      action='store_true',
                      help='Invert exit code.')
  args = parser.parse_args()

  private_paths = _read_private_paths(args.private_paths_file)
  linker_inputs = pathlib.Path(args.linker_inputs).read_text().splitlines()

  root_out_dir = args.root_out_dir
  if root_out_dir == '.':
    root_out_dir = ''

  found = _find_private_paths(linker_inputs, private_paths, root_out_dir)

  if args.allow_violation:
    found, ignored_paths = _apply_allowlist(found, args.allow_violation)
    if ignored_paths:
      print('Ignoring {len(ignored_paths)} allowlisted private paths:')
      _print_paths(sorted(ignored_paths), 10)

  if found:
    limit = 10 if args.expect_failure else 1000
    print(f'Found {len(found)} private paths being linked into public code:')
    _print_paths(found, limit)
  elif args.expect_failure:
    print('Expected to find a private path, but none were found.')

  sys.exit(0 if bool(found) == args.expect_failure else 1)


if __name__ == '__main__':
  main()

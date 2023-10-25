#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script to run action remotely through rewrapper with gn.

Also includes Chromium-specific input processors which don't make sense to
be reclient inbuilt input processors."""

import argparse
import json
import os
import subprocess
import sys
from enum import Enum

_THIS_DIR = os.path.realpath(os.path.dirname(__file__))
_SRC_DIR = os.path.dirname(os.path.dirname(_THIS_DIR))
_MOJOM_DIR = os.path.join(_SRC_DIR, 'mojo', 'public', 'tools', 'mojom')


class CustomProcessor(Enum):
  mojom_parser = 'mojom_parser'

  def __str__(self):
    return self.value


def _normalize_path(path):
  # Always use posix-style directory separators as GN does it.
  return os.path.normpath(path).replace("\\", "/")


def _process_build_metadata_json(bm_file, input_roots, output_root,
                                 output_files, processed_inputs):
  """Recursively find mojom_parser inputs from a build_metadata file."""
  # Import Mojo-specific dep here so non-Mojo remote actions don't need it.
  if _MOJOM_DIR not in sys.path:
    sys.path.insert(0, _MOJOM_DIR)
  from mojom_parser import RebaseAbsolutePath

  if bm_file in processed_inputs:
    return

  processed_inputs.add(bm_file)

  bm_dir = os.path.dirname(bm_file)

  with open(bm_file) as f:
    bm = json.load(f)

  # All sources and corresponding module files are inputs.
  for s in bm["sources"]:
    src = _normalize_path(os.path.join(bm_dir, s))
    if src not in processed_inputs and os.path.exists(src):
      processed_inputs.add(src)
    src_module = _normalize_path(
        os.path.join(
            output_root,
            RebaseAbsolutePath(os.path.abspath(src), input_roots) + "-module"))
    if src_module in output_files:
      continue
    if src_module not in processed_inputs and os.path.exists(src_module):
      processed_inputs.add(src_module)

  # Recurse into build_metadata deps.
  for d in bm["deps"]:
    dep = _normalize_path(os.path.join(bm_dir, d))
    _process_build_metadata_json(dep, input_roots, output_root, output_files,
                                 processed_inputs)


def _get_mojom_parser_inputs(exec_root, output_files, extra_args):
  """Get mojom inputs by walking generated build_metadata files.

  This is less complexity and disk I/O compared to parsing mojom files for
  imports and finding all imports.

  Start from the root build_metadata file passed to mojom_parser's
  --check-imports flag.
  """
  argparser = argparse.ArgumentParser()
  argparser.add_argument('--check-imports', dest='check_imports', required=True)
  argparser.add_argument('--output-root', dest='output_root', required=True)
  argparser.add_argument('--input-root',
                         default=[],
                         action='append',
                         dest='input_root_paths')
  mojom_parser_args, _ = argparser.parse_known_args(args=extra_args)

  input_roots = list(map(os.path.abspath, mojom_parser_args.input_root_paths))
  output_root = os.path.abspath(mojom_parser_args.output_root)
  processed_inputs = set()
  _process_build_metadata_json(mojom_parser_args.check_imports, input_roots,
                               output_root, output_files, processed_inputs)

  # Rebase paths onto rewrapper exec root.
  return map(lambda dep: _normalize_path(os.path.relpath(dep, exec_root)),
             processed_inputs)


def main():
  # Set up argparser with some rewrapper flags.
  argparser = argparse.ArgumentParser(description='rewrapper executor for gn',
                                      allow_abbrev=False)
  argparser.add_argument('--custom_processor',
                         type=CustomProcessor,
                         choices=list(CustomProcessor))
  argparser.add_argument('rewrapper_path')
  argparser.add_argument('--input_list_paths')
  argparser.add_argument('--output_list_paths')
  argparser.add_argument('--exec_root')
  parsed_args, extra_args = argparser.parse_known_args()

  # This script expects to be calling rewrapper.
  args = [parsed_args.rewrapper_path]

  # Get the output files list.
  output_files = set()
  with open(parsed_args.output_list_paths, 'r') as file:
    for line in file:
      # Output files are relative to exec_root.
      output_file = _normalize_path(
          os.path.join(parsed_args.exec_root, line.rstrip('\n')))
      output_files.add(output_file)

  # Scan for and add explicit inputs for rewrapper if necessary.
  # These should be in a new input list paths file, as using --inputs can fail
  # if the list is extremely large.
  if parsed_args.custom_processor == CustomProcessor.mojom_parser:
    root, ext = os.path.splitext(parsed_args.input_list_paths)
    extra_inputs = _get_mojom_parser_inputs(parsed_args.exec_root, output_files,
                                            extra_args)
    extra_input_list_path = '%s__extra%s' % (root, ext)
    with open(extra_input_list_path, 'w') as file:
      with open(parsed_args.input_list_paths, 'r') as inputs:
        file.write(inputs.read())
      file.write("\n".join(extra_inputs))
    args += ["--input_list_paths=%s" % extra_input_list_path]
  else:
    args += ["--input_list_paths=%s" % parsed_args.input_list_paths]

  # Filter out --custom_processor= which is a flag for this script,
  # and filter out --input_list_paths= because we replace it above.
  # Pass on the rest of the args to rewrapper.
  args_rest = filter(lambda arg: '--custom_processor=' not in arg, sys.argv[2:])
  args += filter(lambda arg: '--input_list_paths=' not in arg, args_rest)

  # Run rewrapper.
  proc = subprocess.run(args)
  return proc.returncode


if __name__ == '__main__':
  sys.exit(main())

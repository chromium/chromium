#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Benchmarks the time required to build a single file."""

import argparse
import json
import os
import shutil
import subprocess
import sys

_SCHEMA_DOC = '''{
  "env": {"ENVIRONMENT_VARIABLE_KEY": "ENVIRONMENT VARIABLE VALUE"},
  "configs": {
    "config name": {
      # Flags to add to the command line during this config.
      # This is parsed by bash, so can access environment variables
      "add": "-foo -bar $ENVIRONMENT_VARIABLE_KEY"
    },
    "second config name": {...},
  },
}'''


def error(*args, **kwargs):
  print(*args, **kwargs, file=sys.stderr)
  exit(1)


def run_siso_query(output_dir, file_path):
  """Queries siso for the command that generates the given file."""
  cmd = ['siso', 'query', 'commands', '-C', output_dir, file_path]
  result = subprocess.run(cmd, capture_output=True, check=True)
  # Split the lines before decoding because this can output invalid utf-8, but
  # we only care about the last line being valid.
  command = result.stdout.splitlines()[-1].decode('utf-8').strip()
  return f'cd {output_dir} && {command}'


def main(args, extra_args):
  file_path = args.file
  config = args.config
  includes = set(args.include.split(','))
  includes.discard('')
  excludes = set(args.exclude.split(','))
  excludes.discard('')
  seen = set()

  print(f'Benchmarking build of {file_path}')
  hyperfine = shutil.which('hyperfine')
  if hyperfine is None:
    error(
        'This tool depends on hyperfine. See ' +
        'https://github.com/sharkdp/hyperfine?tab=readme-ov-file#installation')

  cmd = [hyperfine, *extra_args, '--shell=bash']

  def add_command(name, command_str):
    if name not in args.exclude and (name in args.include or not args.include):
      cmd.extend(['-n', name, command_str])
    seen.add(name)

  if not config:
    for output_dir in args.output_dirs:
      build = run_siso_query(output_dir, file_path)
      add_command(output_dir, build)
  elif len(args.output_dirs) != 1:
    error('If --config is provided, only a single -C is supported')
  else:
    build = run_siso_query(args.output_dirs[0], file_path)
    add_command('base', build)

    with open(config) as f:
      cfg = json.load(f)
      for k, v in cfg.get("env", {}).items():
        os.environ[k] = v
      for k, v in cfg["configs"].items():
        new_build = build
        if 'add' in v:
          new_build += ' ' + v['add']
        add_command(k, new_build)

  unused = (includes | excludes) - seen
  if unused:
    error(f'Referring to nonexistent benchmark names: {sorted(unused)}')

  os.execv(cmd[0], cmd)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Benchmark the performance of building a specific file.', )
  parser.add_argument(
      '-C',
      action='append',
      required=True,
      dest='output_dirs',
      help='GN output directory (e.g. out/Default). Can be repeated.',
  )
  parser.add_argument(
      '-f',
      dest='file',
      help='Path to the file to build, relative to the output directory',
      required=True,
  )
  parser.add_argument(
      '--config',
      help='The path to a json file containing configurations. The schema is:\n'
      + _SCHEMA_DOC,
  )
  parser.add_argument(
      '-i',
      '--include',
      default='',
      help='Only run the config with the specified names (comma-seperated)',
  )
  parser.add_argument(
      '-e',
      '--exclude',
      default='',
      help='Skip running the config with the specified names (comma-seperated)',
  )

  main(*parser.parse_known_args())

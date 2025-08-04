#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Modularize modularizes a platform."""

import argparse
import logging
import pathlib
import shutil
import sys
import tempfile
from config import fix_graph
from graph import run_build
from render import render_build_gn
from render import render_modulemap
from compiler import Compiler

SOURCE_ROOT = pathlib.Path(__file__).parents[3].resolve()


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  compiler = Compiler(
      source_root=SOURCE_ROOT,
      gn_out=args.C.resolve(),
      error_dir=None if args.error_log is None else args.error_log.resolve(),
      use_cache=args.cache,
  )
  platform = f'{compiler.os}-{compiler.cpu}'
  logging.info('Detected platform %s', platform)

  out_dir = SOURCE_ROOT / f'build/modules/{platform}'
  # Otherwise gn will error out because it tries to import a file that doesn't exist.
  if not out_dir.is_dir():
    shutil.copytree(out_dir.parent / 'linux-x64', out_dir)

  if args.compile:
    with tempfile.TemporaryDirectory() as td:
      ps, files = compiler.compile_one(args.compile, pathlib.Path(td, 'source'))
      print('stderr:', ps.stderr.decode('utf-8'), file=sys.stderr)
      print('Files used:')
      print('\n'.join(sorted(map(str, files))))
      print('Setting breakpoint to allow further debugging')
      breakpoint()
      return

  graph = compiler.compile_all()
  fix_graph(graph, compiler.os, compiler.cpu)
  targets = run_build(graph)
  out_dir.mkdir(exist_ok=True, parents=False)
  # Since apple provides a modulemap, we only need to create a BUILD.gn file.
  if compiler.os not in ['mac', 'ios']:
    render_modulemap(out_dir=out_dir, sysroot=compiler.sysroot, targets=targets)
  textual_headers = [hdr for hdr in graph.values() if hdr.textual]
  render_build_gn(out_dir=out_dir,
                  textual_headers=textual_headers,
                  targets=targets)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-C', type=pathlib.Path)

  # Make it required so the user understands how compilation works.
  cache = parser.add_mutually_exclusive_group(required=True)
  cache.add_argument(
      '--cache',
      action='store_true',
      help='Enable caching. Will attempt to reuse the compilation results.')
  cache.add_argument(
      '--no-cache',
      action='store_false',
      dest='cache',
      help=
      'Disable caching. Will attempt to recompile the whole libcxx, builtins, and sysroot on every invocation'
  )

  parser.add_argument(
      '--compile',
      help='Compile a single header file',
  )

  parser.add_argument('--error-log',
                      type=lambda value: pathlib.Path(value) if value else None)

  parser.add_argument(
      '--verbosity',
      help='Verbosity of logging',
      default='INFO',
      choices=logging.getLevelNamesMapping().keys(),
      type=lambda x: x.upper(),
  )

  main(parser.parse_args())

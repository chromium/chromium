#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Modularize modularizes a platform."""

import argparse
import concurrent.futures
import logging
import pathlib
import shutil
import subprocess
import sys
import traceback

from config import fix_graph
from graph import IncludeDir
from graph import run_build
import render
from compiler import Compiler

SOURCE_ROOT = pathlib.Path(__file__).parents[3].resolve()


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  if args.C is not None:
    _modularize(
        out_dir=args.C,
        error_log=args.error_log,
        use_cache=args.cache,
        compile=args.compile,
    )
  elif args.all:
    existing_platforms = [
        build.parent.name
        for build in (SOURCE_ROOT / 'build/modules').glob('*-*/BUILD.gn')
    ]
    futures = {}
    # Use a ProcessPoolExecutor rather than a ThreadPoolExecutor because:
    # * No shared state between instances
    # * GIL prevents a performance benefit from a thread pool executor.
    with concurrent.futures.ProcessPoolExecutor() as executor:
      for platform in existing_platforms:
        os, cpu = platform.split('-')
        out_dir = SOURCE_ROOT / 'out' / platform
        args_gn = out_dir / 'args.gn'
        if not args_gn.exists():
          out_dir.mkdir(exist_ok=True, parents=True)
          args_gn.write_text("".join(
              [f'target_os = "{os}"\n'
               f'target_cpu = "{cpu}"\n']))

        error_log = None if args.error_log is None else args.error_log / platform
        futures[platform] = executor.submit(
            _modularize,
            out_dir=out_dir,
            error_log=error_log,
            use_cache=args.cache,
            compile=args.compile,
        )

    success = True
    for platform, future in sorted(futures.items()):
      exc = future.exception()
      if exc is not None:
        success = False
        print(f'{platform} raised an exception:', file=sys.stderr)
        traceback.print_exception(exc)
    if not success:
      exit(1)


def _modularize(out_dir: pathlib.Path, error_log: pathlib.Path | None,
                use_cache: bool, compile: bool):
  # Modularize requires gn gen to have been run at least once.
  if not (out_dir / 'build.ninja').is_file():
    subprocess.run(['gn', 'gen', out_dir], check=True)
  compiler = Compiler(
      source_root=SOURCE_ROOT,
      gn_out=out_dir,
      error_dir=error_log,
      use_cache=use_cache,
  )
  platform = f'{compiler.os}-{compiler.cpu}'
  logging.info('Detected platform %s', platform)

  out_dir = SOURCE_ROOT / f'build/modules/{platform}'
  out_build = out_dir / 'BUILD.gn'
  # Otherwise gn will error out because it tries to import a file that doesn't exist.
  if not out_build.is_file():
    out_dir.mkdir(exist_ok=True)
    shutil.copyfile(out_dir.parent / 'linux-x64/BUILD.gn', out_build)

  if compile:
    ps, files = compiler.compile_one(compile)
    print('stderr:', ps.stderr.decode('utf-8'), file=sys.stderr)
    print('Files used:')
    print('\n'.join(sorted(map(str, files))))
    print('Setting breakpoint to allow further debugging')
    breakpoint()
    return

  graph = compiler.compile_all()
  fix_graph(graph, compiler)
  targets = run_build(graph)
  out_dir.mkdir(exist_ok=True, parents=False)
  if compiler.sysroot_dir == IncludeDir.Sysroot:
    render.render_modulemap(out_dir=out_dir,
                            sysroot=compiler.sysroot,
                            targets=targets)
  render.render_build_gn(
      out_dir=out_dir,
      targets=targets,
      compiler=compiler,
  )


def _optional_path(s: str) -> pathlib.Path | None:
  if s:
    return pathlib.Path(s).resolve()


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  which = parser.add_mutually_exclusive_group(required=True)
  which.add_argument('-C', type=_optional_path)
  which.add_argument(
      '--all',
      action='store_true',
      help='Update all existing platforms',
  )

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
      help=
      'Compile a single header file (eg. --compile=sys/types.h) instead of the whole sysroot. Useful for debugging.',
  )

  parser.add_argument('--error-log', type=_optional_path)

  parser.add_argument(
      '--verbosity',
      help='Verbosity of logging',
      default='INFO',
      choices=logging.getLevelNamesMapping().keys(),
      type=lambda x: x.upper(),
  )

  main(parser.parse_args())

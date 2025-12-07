# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import functools
import logging
import pathlib
import pickle
import re
import shutil
import subprocess
import tempfile
import time

import config
from graph import CompileStatus
from graph import Header
from graph import IncludeDir
from graph import calculate_rdeps
from platforms import Cpu
from platforms import Os
import modulemap

_FRAMEWORK = ' (framework directory)'
# Foo.framework/Versions/A/headers/Bar.h -> Foo/Bar.h
_FRAMEWORK_HEADER = re.compile(
    r'([^/]+)\.framework/(?:Versions/[^/]+/)?(?:Headers|Modules)/(.*)')
_LIBCXXABI = '../../third_party/libc++abi/src/include'


# Some of these steps are quite slow (O(minutes)).
# To allow for fast iteration of config, cache them.
def _maybe_cache(fn):

  @functools.wraps(fn)
  def new_fn(self, *args):
    # The results should be solely dependent on the GN out dir (assuming the
    # user doesn't change args.gn)
    gn_rel = str(self.gn_out.resolve()).lstrip('/')
    cache_path = pathlib.Path(f'/tmp/modularize_cache', gn_rel, fn.__name__,
                              *args)
    cache_path.parent.mkdir(exist_ok=True, parents=True)
    if self._use_cache and cache_path.is_file():
      try:
        return pickle.loads(cache_path.read_bytes())
      # When attempting to run this without a debugger after pickling from a
      # debugger it fails to load pathlib._local.
      except ModuleNotFoundError:
        logging.info('Failed to unpickle - not using cache')
    result = fn(self, *args)
    cache_path.write_bytes(pickle.dumps(result))
    return result

  return new_fn


class Compiler:

  def __init__(self, *, source_root: pathlib.Path, gn_out: pathlib.Path,
               error_dir: pathlib.Path | None, use_cache: bool, os: Os,
               cpu: Cpu):
    self._error_dir = error_dir
    self._use_cache = use_cache
    self.gn_out = gn_out
    self.source_root = source_root

    self.os = os
    self.cpu = cpu
    self.sysroot_dir = IncludeDir.SysrootModule if self.os.is_apple else \
      IncludeDir.Sysroot
    self.sysroot = None

  # __eq__ and __hash__ are required for functools.cache to work correctly.
  def __eq__(self, other):
    return self.gn_out == other.gn_out

  def __hash__(self):
    return hash(self.gn_out)

  def _parse_depfile(self, content: str) -> list[pathlib.Path]:
    deps = content.replace('\\\n', '').split(': ', 1)[1]
    deps = deps.replace('\\ ', ':SPACE:')
    files = []
    # The file will look like:
    # /dev/null: foo.h bar.cc \
    # baz.h \
    # <other includes>
    for dep in deps.split():
      # Remove both the trailing newlines and any escapes in the file names.
      p = pathlib.Path(self.gn_out, dep.replace(':SPACE:', ' ')).resolve()
      if p.suffix != '.txt' and p.suffix != '.cc':
        files.append(p)
    return files

  def _clang_arg(self, arg: str) -> str:
    if self.os == 'win':
      return f'/clang:{arg}'
    else:
      return arg

  def _write_err(self, rel: str, content: bytes):
    if self._error_dir is not None:
      out = self._error_dir / rel
      out.parent.mkdir(exist_ok=True, parents=True)
      out.write_bytes(content)

  def split_path(self, path: pathlib.Path) -> tuple[IncludeDir, str]:
    """Splits a path into the include directory it's under, and the string
    needed to include it from a header file."""
    assert path.is_absolute()
    for d, include_dir in self.include_dirs:
      if path.is_relative_to(d):
        rel = str(path.relative_to(d))
        if include_dir == IncludeDir.Framework:
          framework, hdr = _FRAMEWORK_HEADER.search(rel).groups()
          rel = f'{framework}/{hdr}'
        return include_dir, rel
    raise NotImplementedError(f'Unsupported path {path}')

  # Apply two layers of cache here.
  # The _maybe_cache layer caches between runs via a file.
  # The functools.cache layer ensures you don't keep loading the pickle file.
  @functools.cache
  @_maybe_cache
  def base_command(self) -> list[str]:
    """Returns a command suitable for building for current platform"""
    return subprocess.run(
        [
            'build/modules/modularize/no_modules_compile_command.sh',
            str(self.gn_out),
            str(self.os),
        ],
        check=True,
        text=True,
        cwd=self.source_root,
        stdout=subprocess.PIPE,
        # Strip the -o /dev/null with [:-2]. Windows requires it to be at the
        # end, otherwise it writes to {output}.obj.
    ).stdout.rstrip().replace('\\', '').split(' ')[:-2]

  @functools.cached_property
  def include_dirs(self) -> list[tuple[pathlib.Path, IncludeDir]]:
    cmd = self.base_command() + [
        '-E',
        '-v',
        '-x',
        'c++',
        '-',
        '-o',
        '/dev/null',
    ]
    cmd.remove('/c' if self.os == Os.Win else '-c')
    # include dir lines both start and end with whitespace
    lines = [
        line.strip() for line in subprocess.run(
            cmd,
            cwd=self.gn_out,
            text=True,
            check=True,
            stderr=subprocess.PIPE,
            # We need to pass in a "file" so we pass in - and devnull so it's
            # an empty file.
            stdin=subprocess.DEVNULL,
        ).stderr.replace('\\', '').split('\n')
    ]

    dirs = lines[lines.index('#include <...> search starts here:') +
                 1:lines.index('End of search list.')]
    # We don't care about these.
    dirs.remove('../..')
    dirs.remove('gen')
    if _LIBCXXABI in dirs:
      dirs.remove(_LIBCXXABI)

    out = []
    for d in dirs:
      is_framework = d.endswith(_FRAMEWORK)
      d = (self.gn_out / d.removesuffix(_FRAMEWORK)).resolve()

      if is_framework:
        out.append((d, IncludeDir.Framework))
      elif 'libc++' in d.parts:
        out.append((d, IncludeDir.LibCxx))
      elif 'clang' in d.parts:
        out.append((d, IncludeDir.Builtin))
      elif config.SYSROOT_DIRS.intersection(d.parts):
        out.append((d, self.sysroot_dir))
        self.sysroot = d
      else:
        raise NotImplementedError(f'Unknown include directory {d}')

    return out

  @_maybe_cache
  def compile_one(
      self, include: str
  ) -> tuple[subprocess.CompletedProcess, None | list[pathlib.Path]]:
    """Compiles a single source file including {include}.

    Args:
      include: The string to #include (eg. 'vector')

    Returns:
      The result of the compilation, and either:
        None if no depfile was created,
        A list of all files transitively required otherwise.
    """
    with tempfile.TemporaryDirectory() as td:
      source = pathlib.Path(td, 'source.cc')
      source.write_text(f'#include <{include}>')
      depfile = pathlib.Path(td, 'source.o.d')
      command = self.base_command() + [
          # We write stderr to a file
          '-fno-color-diagnostics',
          '-x',
          'c++-header',
          str(source),
          self._clang_arg('-MD'),
          self._clang_arg('-MF'),
          self._clang_arg(depfile),
          '-o',
          '/dev/null',
      ]
      if logging.getLogger().isEnabledFor(logging.DEBUG):
        logging.debug('Running command: (cd %s && %s)', self.gn_out,
                      ' '.join(map(str, command)))
      ps = subprocess.run(
          command,
          stderr=subprocess.PIPE,
          cwd=self.gn_out,
      )
      # The depfile is generated even if it fails to compile.
      try:
        return ps, self._parse_depfile(depfile.read_text())
      except FileNotFoundError:
        return ps, None

  @functools.cache
  def _modules_and_headers(self):
    return modulemap.calculate_modules(self.include_dirs)

  def modulemaps_for_modules(self) -> dict[str, pathlib.Path]:
    return self._modules_and_headers()[0]

  def modulemap_headers(self) -> set[Header]:
    return self._modules_and_headers()[1]

  @_maybe_cache
  def compile_all(self) -> dict[str, Header]:
    """Generates a graph of headers by compiling all files in the sysroot."""
    if self._error_dir is not None:
      shutil.rmtree(self._error_dir, ignore_errors=True)

    graph: dict[str, Header] = {}
    uncompiled = []
    seen = set()

    def visit(include: str):
      if include not in seen:
        uncompiled.append(include)
        seen.add(include)

    for hdr in self.modulemap_headers():
      if hdr.root_module not in config.IGNORED_MODULES:
        graph[(hdr.include_dir, hdr.rel)] = hdr
        visit(hdr.rel)

    # Populate a list of initial headers to compile.
    for hdr in config.SYSROOT_PRECOMPILED_HEADERS:
      visit(hdr)

    logging.info('Starting compilation')

    # Could consider making the DFS parallel to improve performance.
    # But it's a lot of effort for a script that's rarely run.
    i = 0
    start = time.time()
    while uncompiled:
      i += 1
      if i % 100 == 0:
        rate = i / (time.time() - start)
        logging.info('Compiled %d/%d, %.2f/s, estimate: %ds', i - 1, len(seen),
                     rate, (len(seen) - i) / rate)
      rel = uncompiled.pop()

      ps, files = self.compile_one(rel)
      if files is None:
        logging.warning("Failed to generate depfile while compiling %s", rel)
        self._write_err(rel, ps.stderr)
        continue

      abs_path = files[0]
      kind, _ = self.split_path(abs_path)

      if (kind, rel) not in graph:
        # If we're seeing it for the first time here, but it's from another
        # include dir, it must not be in the module map, so it should be treated
        # as textual.
        graph[(kind, rel)] = Header(include_dir=kind,
                                    rel=rel,
                                    textual=kind != IncludeDir.Sysroot)
      state = graph[(kind, rel)]
      state.abs = abs_path

      for to_abs in files[1:]:
        to_kind, to_rel = self.split_path(to_abs)
        assert (kind, rel) != (to_kind, to_rel)
        dep = graph.get((to_kind, to_rel), None)
        if dep is None:
          dep = Header(
              include_dir=to_kind,
              rel=to_rel,
              abs=to_abs,
              textual=to_kind != IncludeDir.Sysroot,
          )
          graph[(to_kind, to_rel)] = dep
          # Skip compiling textual headers - we'll calculate their dependencies
          # after the fact.
          if not dep.textual:
            visit(to_rel)
        state.deps.append(dep)

      state.compile_status = CompileStatus.Success if ps.returncode == 0 else \
        CompileStatus.Failure
      if ps.returncode == 0:
        logging.debug('Compiled %s', state.pretty_name)
      elif any([
          state.textual,
          rel.startswith('bits/'),
          '/bits/' in rel,
          rel.endswith('intrin.h'),
          b'Do not include this header directly' in ps.stderr,
          # eg. Please #include <os/workgroup.h> instead of this file directly.
          b'Please #include' in ps.stderr,
      ]):
        # These things are generally expected to not compile standalone.
        logging.debug('Probably fine: Failed to compile %s', state.pretty_name)
      else:
        if state.root_module != None:
          logging.warning('%s was not textual but failed to compile',
                          state.pretty_name)
        else:
          # Since this isn't part of a modulemap we can choose to mark it as
          # textual.
          logging.warning('Failed to compile %s', state.pretty_name)
        self._write_err(rel, ps.stderr)

      # If you can't compile it, assume it's textual
      if state.root_module is None and ps.returncode != 0:
        state.textual = True

    rdeps = calculate_rdeps(graph.values())
    includes = collections.defaultdict(list)

    logging.info('Inferring dependencies')
    for header in sorted(graph.values()):
      includes[header.rel].append(header)
      if header.abs is None:
        for d, kind in self.include_dirs:
          if header.include_dir == kind and (d / header.rel).is_file():
            header.abs = d / header.rel
            break
      assert header.abs is not None

      # If we were unable to compile something, calculate what the dependencies
      # likely are.
      if header.compile_status == CompileStatus.NotCompiled and rdeps[header]:
        intersection = set.intersection(
            *[set(rdep.deps) for rdep in rdeps[header]])
        # For libcxx/foo.h -> builtin/foo.h -> sysroot/foo.h
        # Despite the fact that builtin/foo.h should appear all the time, we
        # need to filter it out for sysroot/foo.h.
        header.deps = [
            dep for dep in intersection
            if dep.rel != header.rel or dep.include_dir > header.include_dir
        ]

    # Translate it to a mapping from include path to a linked list of headers.
    out = {}
    for k, headers in includes.items():
      headers.sort()
      for prev, nxt in zip(headers, headers[1:]):
        # If it didn't #include_next we don't need to worry about it.
        if nxt not in prev.deps:
          break
        prev.next = nxt
        nxt.prev = prev
      out[k] = headers[0]

    logging.info('Compilation complete')
    return out

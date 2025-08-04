# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import functools
import hashlib
import itertools
import logging
import pathlib
import pickle
import re
import shutil
import subprocess
import sys
import tempfile

from graph import IncludeDir
from graph import Header
from graph import HeaderRef

_MODULE_START = re.compile('^module ([a-z0-9_]+) ', flags=re.I)
_HEADER = re.compile('( textual)? header "([^"]*)"')

# It doesn't matter if these don't work on all platforms.
# It'll just print a warning saying it failed to compile.
# This contains a list of files that aren't depended on by libc++, but we still
# want to precompile.
_SYSROOT_PRECOMPILED_HEADERS = ['fcntl.h']


# Some of these steps are quite slow (O(minutes)).
# To allow for fast iteration of config, cache them.
def _maybe_cache(fn):

  @functools.wraps(fn)
  def new_fn(self, *args, **kwargs):
    # The results should be solely dependent on the GN out dir (assuming the
    # user doesn't change args.gn)
    gn_rel = str(self.gn_out.resolve()).lstrip('/')
    cache_path = pathlib.Path(f'/tmp/modularize_cache', gn_rel, fn.__name__)
    cache_path.parent.mkdir(exist_ok=True, parents=True)
    if self._use_cache and cache_path.is_file():
      return pickle.loads(cache_path.read_bytes())
    result = fn(self, *args, **kwargs)
    cache_path.write_bytes(pickle.dumps(result))
    return result

  return new_fn


# We don't need a true parse, just want to determine which modules correspond
# to which files.
def _parse_modulemap(path: pathlib.Path) -> dict[str, list[tuple[str, bool]]]:
  """Parses a modulemap into name -> [(header, textual)]"""
  modules = collections.defaultdict(list)
  with path.open() as f:
    for line in f:
      mod = _MODULE_START.match(line)
      if mod is not None:
        current_module = mod.group(1)
      header = _HEADER.search(line)
      if header is not None:
        modules[current_module].append((header.group(2), bool(header.group(1))))
  # This is a builtin module with feature requirements.
  modules.pop('opencl_c', None)
  return modules


class Compiler:

  def __init__(self, *, source_root: pathlib.Path, gn_out: pathlib.Path,
               error_dir: pathlib.Path | None, use_cache: bool):
    self._error_dir = error_dir
    self._use_cache = use_cache
    self.gn_out = gn_out
    self.source_root = source_root

    self.os = self._get_os()
    self.cpu = self._get_cpu()

    if self.os == 'linux':
      self.sysroot = self.source_root / 'build/linux/debian_bullseye_amd64-sysroot/usr/include'
    elif self.os == 'android':
      self.sysroot = self.source_root / 'third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include'
    else:
      self.sysroot = list(
          source_root.glob(
              'third_party/depot_tools/win_toolchain/vs_files/*/Windows Kits/10/Include/*'
          ))[0]
      self.msvc_dir = list(
          source_root.glob(
              'third_party/depot_tools/win_toolchain/vs_files/*/VC/Tools/MSVC/*/include'
          ))[0]

  def _parse_depfile(self, content: str) -> list[pathlib.Path]:
    files = []
    # The file will look like:
    # /dev/null: <blah>.cc \
    # <main include> \
    # <other includes> \
    # So we need [1:] to ensure it doesn't have a dependency on itself.
    for line in content.rstrip().split('\n')[1:]:
      # Remove both the trailing newlines and any escapes in the file names.
      files.append(
          pathlib.Path(self.gn_out,
                       line.replace('\\', '').strip(' ')).resolve())
    return files

  def _get_gn_arg(self, name: str) -> str:
    content = (self.gn_out / 'args.gn').read_text()
    ps = subprocess.run(
        ['gn', 'args', '.', f'--list={name}', '--short'],
        text=True,
        check=False,
        cwd=self.gn_out,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )

    # GN args outputs errors to stdout, so we can't use check=True.
    if ps.returncode != 0:
      print(ps.stdout, file=sys.stderr)
      exit(1)

    # output format: 'target_cpu = "x64"\n'
    return ps.stdout.rstrip().split(' = ')[1].strip('"')

  def _clang_arg(self, arg: str) -> str:
    if self.os == 'win':
      return f'/clang:{arg}'
    else:
      return arg

  @_maybe_cache
  def _get_cpu(self):
    # If the target_cpu is not explicitly set, it returns the empty string and
    # it uses the host_cpu instead.
    return self._get_gn_arg('target_cpu') or self._get_gn_arg('host_cpu')

  @_maybe_cache
  def _get_os(self):
    # If the target_os is not explicitly set, it returns the empty string and
    # it uses the host_os instead.
    return self._get_gn_arg('target_os') or self._get_gn_arg('host_os')

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
        return include_dir, str(path.relative_to(d))

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
            self.os,
        ],
        check=True,
        text=True,
        cwd=self.source_root,
        stdout=subprocess.PIPE,
        # Strip the -o /dev/null with [:-2]
        # Windows requires it to be at the end, otherwise it writes to {output}.obj.
    ).stdout.rstrip().replace('\\', '').split(' ')[:-2]

  # Again, two layers of cache here to cache between runs and within a run.
  @functools.cached_property
  @_maybe_cache
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
    cmd.remove('-c')
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
    dirs.remove('../../third_party/libc++abi/src/include')

    out = []
    for d in dirs:
      d = (self.gn_out / d).resolve()
      if d.is_relative_to(self.sysroot):
        out.append((d, IncludeDir.Sysroot))
      elif 'libc++' in d.parts:
        out.append((d, IncludeDir.LibCxx))
      elif 'clang' in d.parts:
        out.append((d, IncludeDir.Builtin))
      else:
        raise NotImplementedError(f'Unknown include directory {d}')

    return out

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
                      ' '.join(command))
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

  @_maybe_cache
  def compile_all(self) -> dict[HeaderRef, Header]:
    """Generates a graph of headers by compiling all files in the sysroot."""
    if self._error_dir is not None:
      shutil.rmtree(self._error_dir, ignore_errors=True)

    graph: dict[HeaderRef, Header] = {}
    uncompiled = []
    seen = set()

    def visit(include: str):
      if include not in seen:
        uncompiled.append(include)
        seen.add(include)

    # Use a list as a set because it's tiny.
    seen_dirs = [IncludeDir.Sysroot]

    def add_to_dfs(kind: IncludeDir, modulemap: pathlib.Path):
      if kind in seen_dirs:
        return
      seen_dirs.append(kind)
      for mod, files in _parse_modulemap(modulemap).items():
        for path, textual in files:
          graph[(kind, path)] = Header(include_dir=kind,
                                       rel=path,
                                       root_module=mod,
                                       textual=textual)
          visit(path)

    # Populate a list of initial headers to compile.
    add_to_dfs(
        IncludeDir.LibCxx,
        self.source_root / 'third_party/libc++/src/include/module.modulemap.in')
    for header in _SYSROOT_PRECOMPILED_HEADERS:
      visit(header)

    # Could consider making the DFS parallel to improve performance.
    # But it's a lot of effort for a script that's rarely run.
    while uncompiled:
      rel = uncompiled.pop()

      ps, files = self.compile_one(rel)
      if files is None:
        logging.warning("Failed to generate depfile while compiling %s", rel)
        self._write_err(rel, ps.stderr)
        continue

      abs_path = files[0]
      kind, _ = self.split_path(abs_path)

      # The first time we come across a builtin header, we use that to find
      # the builtin modulemap to ensure we compile every module in it.
      add_to_dfs(
          kind,
          abs_path.parents[rel.count('/')] / 'module.modulemap',
      )

      if (kind, rel) not in graph:
        # If we're seeing it for the first time here, but it's from another
        # include dir, it must not be in the module map, so it should be treated as textual.
        graph[(kind, rel)] = Header(include_dir=kind,
                                    rel=rel,
                                    textual=kind != IncludeDir.Sysroot)
      state = graph[(kind, rel)]
      state.abs = abs_path

      for to_abs in files[1:]:
        to_kind, to_rel = self.split_path(to_abs)
        assert (kind, rel) != (to_kind, to_rel)
        if (to_kind, to_rel) not in graph:
          graph[(to_kind, to_rel)] = Header(
              include_dir=to_kind,
              rel=to_rel,
              abs=to_abs,
              textual=to_kind != IncludeDir.Sysroot,
          )
        state.deps.append((to_kind, to_rel))
        visit(to_rel)

      if ps.returncode == 0:
        logging.debug('Compiled %s', state.pretty_name)
      elif any([
          state.textual,
          rel.startswith('bits/'), '/bits/' in rel,
          rel.endswith('intrin.h')
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

    assert IncludeDir.Builtin in seen_dirs

    for header in graph.values():
      if header.abs is None:
        for d, kind in self.include_dirs:
          if header.include_dir == kind and (d / header.rel).is_file():
            header.abs = d / header.rel
            break
      assert header.abs is not None

      for dep in header.deps:
        assert dep in graph

    return graph

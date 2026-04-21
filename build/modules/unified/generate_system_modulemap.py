#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Dynamically generates a modulemap for a sysroot + libcxx + clang."""

import argparse
import dataclasses
import io
import itertools
import os
import pathlib
import re
import shlex
import subprocess
import tempfile
from typing import Tuple, List

_HEADER_RE = re.compile(r'(?:(private)\s+)?(?:(textual)\s+)?header\s+"([^"]+)"')
_SIMPLE_HEADER_RE = re.compile(r'(\bheader\s+")([^"]+)(")')
_REQUIRES_RE = re.compile(r'^\s*requires\s+(.*)')
_STRIP_PREFIX = re.compile(r'^usr/include/(?:(?:x86_64)-(?:linux|cros)-gnu/)?')
_CPU_ARG = {
    'x64': 'x86_64',
}


# Path.absolute() only exists in python 3.11, gmacs still have python3.9
# Similar to Path.resolve() but doesn't follow symlinks.
def _absolute(p: pathlib.Path) -> pathlib.Path:
  return pathlib.Path(os.path.abspath(p))


# These headers are known to be textual.
_FORCE_TEXTUAL = {
    # Inherently textual
    'assert.h',
    'stddef.h',
    'stdio.h',
    'stdlib.h',

    # Include loop with features.h
    'sys/cdefs.h',

    # #include_next works differently with modules
    'wchar.h',

    # Required to be textual to compile properly.
    'android/legacy_stdlib_inlines.h',
    'android/legacy_threads_inlines.h',
    'asm-generic/bitsperlong.h',
    'asm-generic/posix_types.h',
    'asm-generic/unistd.h',
    'asm/posix_types.h',
    'bits/threads_inlines.h',

    # See https://codebrowser.dev/glibc/glibc/sysdeps/unix/sysv/linux/bits/local_lim.h.html#56
    # if linux/limits.h is non-textual, then limits.h undefs the limits.h
    # defined in the linux/limits.h module.
    # Thus, limits.h exports an undef.
    # if it's textual, limits.h undefs something it defined itself.
    'linux/limits.h',
    'limits.h',
}

# We consider sysroot headers to be, by default, private.
# If we defaulted to public, a header included transitively in one configuration
# but not another, or one that is no longer depended on by libc++ / elsewhere in
# the sysroot would be removed from the modulemap.
# This would definitely break builds with the layering check enabled, and has
# the potential for extremely confusing errors without it.
#
# Thus, this is a list of *every* sysroot file directly included by chromium to
# be precompiled.
_ALLOWLIST_PATH = pathlib.Path(
    __file__).parent / '../../include_sysroot_allowlist.txt'
_PUBLIC_SYSROOT_HEADERS = [
    line.strip() for line in _ALLOWLIST_PATH.read_text().split('\n')
    if line and not line.startswith("#")
]

# Disabled headers are present in modulemaps, but we should assume that they are
# unable to be used (note that this inherently means they must be textual).
_DISABLED_HEADERS = {
    'mm3dnow.h',  # Deprecated
}


def _requires_met(require: str, os: str, cpu: str) -> bool:
  """Returns whether a modulemap requires condition is met."""
  invert = require.startswith('!')
  require = require.lstrip('!')

  met = False
  if require == 'altivec':
    # Triggered by -maltivec. Not used in chromium.
    met = False
  elif require == 'arm':
    met = cpu == 'arm'
  elif require == 'arm64':
    met = cpu == 'arm64'
  elif require == 'freestanding':
    # Chromium is never freestanding.
    met = False
  elif require == 'gnuinlineasm':
    # Enabled by default unless -fno-gnu-inline-asm set.
    met = True
  elif require == 'opencl':
    # Not used in chromium.
    met = False
  elif require == 'systemz':
    met = cpu == 's390x'
  elif require == 'x86':
    met = cpu in ['x86', 'x64']
  else:
    raise NotImplementedError(f'Unknown require: {require}')

  return not met if invert else met


@dataclasses.dataclass(order=True)
class Header:
  """Represents a header file declaring its properties in a modulemap."""
  path: pathlib.Path
  private: bool
  textual: bool
  requires: list[str] = dataclasses.field(default_factory=list)


def _should_be_textual(short_path: pathlib.Path) -> bool:
  """Returns whether a header should be marked textual."""
  if 'bits' in short_path.parts:
    return True
  return str(short_path) in _FORCE_TEXTUAL


def parse_modulemap(
    modulemap_path: pathlib.Path) -> Tuple[pathlib.Path, List[Header]]:
  """Parses a modulemap file into headers.

  Args:
    modulemap_path: Path to the modulemap file.

  Returns:
    A tuple of (include_dir, headers relative to that directory).
  """
  matches = []
  # A stack of modules, each with their own requirements.
  requires_stack = []

  with open(modulemap_path) as f:
    for line in f:
      if '{' in line:
        requires_stack.append([])

      m_req = _REQUIRES_RE.search(line)
      if m_req:
        req_str = m_req.group(1).split('//')[0].strip()
        reqs = [r.strip() for r in req_str.split(',') if r.strip()]
        requires_stack[-1].extend(reqs)

      m = _HEADER_RE.search(line)
      if m:
        matches.append((
            m.group(3),
            bool(m.group(1)),
            bool(m.group(2)),
            itertools.chain(*requires_stack),
        ))

      if '}' in line:
        requires_stack.pop()

  assert not requires_stack
  common_prefix = os.path.commonpath([m[0] for m in matches])
  include_dir = _absolute(modulemap_path.parent / common_prefix)

  headers = []
  for rel, is_private, is_textual, reqs in matches:
    path = pathlib.Path(rel[len(common_prefix):].lstrip('/'))
    # libc++ uses __ prefixes instead of private headers.
    is_private |= any(part.startswith('__') for part in path.parts)
    headers.append(
        Header(
            path=path,
            private=is_private,
            textual=is_textual,
            requires=list(reqs),
        ))
  return include_dir, headers


def calculate_transitive_headers(clang_args: list[str],
                                 include_dirs: List[Tuple[pathlib.Path,
                                                          List[Header]]],
                                 sysroot: pathlib.Path,
                                 extra_public_headers: list[str],
                                 target_os: str,
                                 target_cpu: str) -> List[Header]:
  """Runs Clang to discover transitive dependencies from the provided headers.

  Returns a list of all headers discovered that are part of the sysroot.
  """
  sysroot = _absolute(sysroot)
  with tempfile.TemporaryDirectory() as tmpdir:
    tmpdir_path = pathlib.Path(tmpdir)
    source_file = tmpdir_path / 'dummy.cpp'
    dep_file = tmpdir_path / 'dummy.d'
    input_headers = {source_file.absolute()}

    with open(source_file, 'w') as f:
      for include_dir, headers in include_dirs:
        for h in headers:
          if not h.private and str(h.path) not in _DISABLED_HEADERS:
            if all(_requires_met(r, target_os, target_cpu) for r in h.requires):
              f.write(f'#include <{h.path}>\n')
          input_headers.add(include_dir / h.path)
      for h in extra_public_headers:
        # Intentionally do this so that headers such as android/* just do
        # nothing on non-android platforms instead of erroring out.
        f.write(f'#if __has_include(<{h}>)\n')
        f.write(f'#include <{h}>\n')
        f.write(f'#endif\n')

    cmd = clang_args + [
        # We only need to preprocess for performance reasons, and don't even
        # care about the preprocessed output.
        '-E',
        str(source_file),
        '-o',
        '/dev/null',
        # This is what we really care about. Just which headers were in the
        # transitive includes of a given header.
        '-MD',
        '-MF',
        str(dep_file),
    ]

    subprocess.run(cmd, check=True)

    dep_content = dep_file.read_text().replace('\\\n', '')
    deps = dep_content.split(': ', 1)[1].split()

    extra_public_headers = set(extra_public_headers)
    headers = []
    for dep in deps:
      full = _absolute(dep)
      if full in input_headers:
        # We don't need to add this to the modulemap ourselves.
        continue

      try:
        rel = str(full.relative_to(sysroot))
        rel = _STRIP_PREFIX.sub('', rel)
        private = rel not in extra_public_headers
        textual = _should_be_textual(pathlib.Path(rel))
      except ValueError:
        # relative_to raises ValueError if it's outside the sysroot
        # It must be incorrectly missing from the modulemap.
        private = full.name not in extra_public_headers
        # This has the same effect as it being missing from the modulemap.
        textual = True
      headers.append(Header(path=full, private=private, textual=textual))

    return headers


def combine_modulemaps(out: pathlib.Path, modulemaps: list[pathlib.Path],
                       headers: List[Header], module_name: str) -> str:
  """Generates the combined modulemap output string from dependencies."""
  custom_header_prefix = os.path.relpath('../../buildtools/third_party/libc++',
                                         out.parent)

  with io.StringIO() as s:
    modules = {'system': 1}
    s.write(f'module "{module_name}" [system] {{\n')
    for mm in modulemaps:
      prefix = os.path.relpath(mm.parent, out.parent)

      def rebase_path(p: str) -> str:
        if p == '__assertion_handler':
          return f'{custom_header_prefix}/__assertion_handler'
        return os.path.normpath(os.path.join(prefix, p))

      mm_content = _SIMPLE_HEADER_RE.sub(
          lambda m: f'{m.group(1)}{rebase_path(m.group(2))}{m.group(3)}',
          mm.read_text())
      mm_content = mm_content.replace(
          '@LIBCXX_CONFIG_SITE_MODULE_ENTRY@ // generated via CMake',
          f'textual header "{custom_header_prefix}/__config_site"')
      s.write(mm_content)
      s.write('\n')

    for header in headers:
      header.path = pathlib.Path(os.path.relpath(header.path, out.parent))
    # Sort by path for determinism.
    headers.sort()

    for header in headers:
      private = 'private ' if header.private else ''
      textual = 'textual ' if header.textual else ''
      # The module name of submodules is arbitrary and doesn't matter.
      # Only the root module's name matters.
      if header.path.name in modules:
        modules[header.path.name] += 1
        # We've already seen a file with the same name, so rename the module
        # name to avoid naming conflicts.
        module_name = f'{header.path.name}_{modules[header.path.name]}'
      else:
        modules[header.path.name] = 1
        module_name = header.path.name
      s.write(f'module "{module_name}" {{\n')
      s.write(f'  {private}{textual}header "{header.path}"\n')
      s.write('  export *\n')
      s.write('}\n')

    s.write('}\n')
    return s.getvalue()


def main(args):
  """Executes the modulemap generation pipeline."""
  clang_cpu = _CPU_ARG[args.cpu]
  deps = calculate_transitive_headers(
      clang_args=[
          str(args.clang),
          # Some files are only read with optimization flags enabled.
          '-O2',
          '-D_FORTIFY_SOURCE=3',
          # Target architecture is required for preprocessor to define built-in
          # target-specific macros (e.g., __x86_64__).
          f'--target={clang_cpu}-unknown-{args.os}-gnu',
          f'--sysroot={args.sysroot}',
          # Ensure we're using the right libc++
          '-nostdinc++',
          '-I../../third_party/libc++/src/include',
          '-I../../third_party/libc++abi/src/include',
          '-I../../buildtools/third_party/libc++',
          # Libc++ feature/hardening macros required by libc++ headers.
          '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE',
          '-D_LIBCPP_BUILDING_LIBRARY',
          '-std=c++23',
          # Ensures that paths to compiler builtin headers are kept relative
          # rather than being resolved to absolute/canonical symlinked paths.
          '-no-canonical-prefixes',
      ],
      include_dirs=[parse_modulemap(mm) for mm in args.modulemap],
      sysroot=args.sysroot,
      extra_public_headers=_PUBLIC_SYSROOT_HEADERS,
      target_os=args.os,
      target_cpu=args.cpu,
  )

  out_str = combine_modulemaps(out=args.output,
                               modulemaps=args.modulemap,
                               headers=deps,
                               module_name=args.module_name)
  args.output.write_text(out_str)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Generate a system modulemap using clang to discover deps')
  parser.add_argument('--clang',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the Clang compiler binary.')
  parser.add_argument('--sysroot',
                      type=pathlib.Path,
                      required=True,
                      help='Path to the sysroot directory.')
  parser.add_argument('--output',
                      type=pathlib.Path,
                      required=True,
                      help='Path where the merged modulemap will be written.')
  parser.add_argument('--module-name', required=True, help='Name of the module')
  parser.add_argument(
      '--modulemap',
      action='append',
      type=pathlib.Path,
      required=True,
      help='Path to a modulemap to merge. Can be specified multiple times.')
  parser.add_argument('--os', required=True, help="GN's $target_os variable")
  parser.add_argument('--cpu', required=True, help="GN's $target_cpu variable")
  main(parser.parse_args())

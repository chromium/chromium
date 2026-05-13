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
import shutil
import sys
import subprocess
import tempfile
from typing import Tuple, List

_HEADER_RE = re.compile(r'(?:(private)\s+)?(?:(textual)\s+)?header\s+"([^"]+)"')
_SIMPLE_HEADER_RE = re.compile(r'(\bheader\s+")([^"]+)(")')
_REQUIRES_RE = re.compile(r'^\s*requires\s+(.*)')
# This needs to match all triple dirs that exist in /usr/include in the sysroot.
_TRIPLE = re.compile('^(.*)-(linux|cros)-(gnu|gnueabi|gnueabihf|android)$')
_DEBUG_SOURCE = '/tmp/debug_generate_system_modulemap.cc'
_DEBUG_SCRIPT = pathlib.Path('/tmp/debug_generate_system_modulemap.sh')

# Pre-C++23, libc++'s stdatomic.h includes the builtin stdatomic.h
# But since then, it does not, and thus _Builtin_stdatomic is inaccessible.
# This tool correctly says not to include the sysroot's stdatomic.h, but it is
# used by the inaccessible _Builtin_stdatomic module, so we need to manually
# remove it so it compiles.
_STRIP = re.compile(r'\nmodule _Builtin_stdatomic \[system\] \{.*?\}\n',
                    re.MULTILINE | re.DOTALL)

_MODULEMAP_START = re.compile(
    r'^\b(?:(?:explicit|framework)\s+)*module\s+"?([^" ]+)"?\s.*\{',
    re.MULTILINE)


# Path.absolute() only exists in python 3.11, gmacs still have python3.9
# Similar to Path.resolve() but doesn't follow symlinks.
def _absolute(p: pathlib.Path) -> pathlib.Path:
  return pathlib.Path(os.path.abspath(p))


def _format_clang_args(args, os):
  if os != 'win':
    return args
  else:
    # clang-cl takes args such as /I and rewrites them to -I for clang.
    # If you provide -clang: it prevents the rewrite.
    return [f'-clang:{arg}' for arg in args]


# Usually ../.., but not always.
_SRC_PREFIX = pathlib.Path(
    os.path.relpath(pathlib.Path(__file__).parents[3], os.getcwd()))


# We consider sysroot headers to be, by default, private.
# If we defaulted to public, a header included transitively in one configuration
# but not another, or one that is no longer depended on by libc++ / elsewhere in
# the sysroot would be removed from the modulemap.
# This would definitely break builds with the layering check enabled, and has
# the potential for extremely confusing errors without it.
#
# Thus, this is a list of *every* sysroot file directly included by chromium to
# be precompiled.
def parse_allowlist():
  path = _SRC_PREFIX / 'build/include_sysroot_allowlist.txt'
  lines = [
      line.strip() for line in path.read_text().split('\n')
      if line and not line.startswith('#')
  ]
  force_textual = {}
  allowlist = {}
  last_path = None
  for line in lines:
    path, *attrs = line.split(', ')
    if last_path is not None and path <= last_path:
      raise ValueError(
          f'Allowlist is not sorted. {path} should be before {last_path}')
    last_path = path

    textual = None
    lazy = None
    for attr in attrs:
      if attr == 'textual':
        textual = 'True'
      elif attr.startswith('textual='):
        textual = attr[8:]
      elif attr == 'lazy':
        lazy = 'True'
      elif attr.startswith('lazy='):
        lazy = attr[5:]
      else:
        raise ValueError(
            f'Unknown attribute {repr(attr)} in allowlist for {path}')
    allowlist[path] = lazy
    if textual is not None:
      force_textual[path] = textual

  return allowlist, force_textual


_HEADERS, _FORCE_TEXTUAL = parse_allowlist()

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
    met = cpu == 'arm' or cpu == 'arm64'
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
  elif require == 'windows':
    met = os == 'win'
  elif require == 'neon':
    # Required for 64-bit arm, unsure about 32-bit
    met = cpu == 'arm64'
  elif require == 'sve':
    # Assume chrome doesn't use SVE
    met = False
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


def split_modulemap(modulemap: str) -> dict[str, str]:
  results = {}
  upto = 0
  for m in _MODULEMAP_START.finditer(modulemap):
    # This is a submodule of a previously referenced module
    if m.start() < upto:
      continue
    depth = 0
    for c in range(m.start(), len(modulemap)):
      # Brace counting is pretty rudimentary, and could fail in theory.
      # However, in practice, they seem to work pretty well.
      # A user is highly unlikely to write a "{" without a "}" in a comment, for
      # example.
      if modulemap[c] == '{':
        depth += 1
      elif modulemap[c] == '}':
        depth -= 1
        if depth == 0:
          upto = c + 1
          results[m.group(1)] = modulemap[m.start():upto]
          break
  return results


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


def parse_depfile(content: str) -> list[pathlib.Path]:
  """Parses the contents of a Clang-generated depfile.

  Returns:
    A list of dependency file paths as pathlib.Path objects.
  """
  # We know there'll only ever be one thing we're trying to build.
  content = content.replace('\\\n', '').split(': ', 1)[1]
  deps = []
  current = []
  # Spaces in file names occur in the windows sysroot, so we need to handle
  # them.
  for part in content.split():
    if part.endswith('\\'):
      current.append(part[:-1])
    else:
      current.append(part)
      deps.append(pathlib.Path(' '.join(current)))
      current = []
  return deps


def calculate_transitive_headers(clang_args: list[str],
                                 include_dirs: list[Tuple[pathlib.Path,
                                                          List[Header]]],
                                 sysroot_dirs: list[pathlib.Path],
                                 extra_public_headers: list[str],
                                 target_os: str,
                                 target_cpu: str,
                                 debug: bool = False) -> list[Header]:
  """Runs Clang to discover transitive dependencies from the provided headers.

  Returns a list of all headers discovered that are part of the sysroot.
  """
  # Sort in reverse order to make sure that foo/bar comes before foo, thus
  # ensuring we try resolving foo/bar/baz => baz instead of bar/baz.
  sysroot_dirs = sorted([_absolute(d) for d in sysroot_dirs],
                        key=lambda p: len(p.parts),
                        reverse=True)
  context = {
      'is_linux': target_os == 'linux',
      'is_android': target_os == 'android',
      'is_ios': target_os == 'ios',
      'is_mac': target_os == 'mac',
      'is_apple': target_os == 'mac' or target_os == 'ios',
      'is_fuchsia': target_os == 'fuchsia',
      'is_win': target_os == 'win',
  }
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
      for h, lazy in extra_public_headers.items():
        # Intentionally do this so that headers such as android/* just do
        # nothing on non-android platforms instead of erroring out.
        if lazy is None or not eval(lazy, context):
          f.write(f'#if __has_include(<{h}>)\n')
          f.write(f'#include <{h}>\n')
          f.write(f'#endif\n')

    # We only need to preprocess for performance reasons, and don't even
    # care about the preprocessed output.
    cmd = [
        *clang_args,
        *_format_clang_args(['-E'], target_os),
        str(source_file),
        *_format_clang_args(
            [
                '-o',
                'NUL' if os.name == 'nt' else '/dev/null',
                # This is what we really care about. Just which headers were in
                # the transitive includes of a given header.
                '-MD',
                '-MF',
                str(dep_file),
            ],
            target_os)
    ]

    if debug:
      shutil.copyfile(source_file, _DEBUG_SOURCE)

      content = f"""#!/bin/bash
      cd "{os.getcwd()}"
      {shlex.join(cmd)}
"""
      content = content.replace(str(source_file), _DEBUG_SOURCE)
      content = content.replace(str(dep_file), _DEBUG_SOURCE + '.o.d')
      _DEBUG_SCRIPT.write_text(content)
      _DEBUG_SCRIPT.chmod(0o755)
      print(f'Saved debug script to {_DEBUG_SCRIPT}')
      sys.exit(0)

    ps = subprocess.run(cmd, check=False)
    if ps.returncode != 0:
      print(f'Suggestion: Run `cd {os.getcwd()} && {sys.argv[0]} --debug',
            f'{shlex.join(sys.argv[1:])}` to debug')
      sys.exit(ps.returncode)

    deps = parse_depfile(dep_file.read_text())

    extra_public_headers = set(extra_public_headers)
    headers = []
    for dep in deps:
      full = _absolute(dep)
      if full in input_headers:
        # We don't need to add this to the modulemap ourselves.
        continue

      private = None
      # This has the same effect as it being missing from the modulemap.
      textual = True
      for d in sysroot_dirs:
        if full.is_relative_to(d):
          rel = str(full.relative_to(d))
          private = rel not in extra_public_headers
          if rel in _FORCE_TEXTUAL:
            textual = eval(_FORCE_TEXTUAL[rel], context)
          else:
            textual = 'bits' in pathlib.Path(rel).parts
          break
      if private is None:
        # It's not relative to any sysroot directory. It must be either from
        # libc++ or builtins.
        # In either case it's incorrectly missing from the modulemap, so we mark
        # it as private & textual by default to match existing behaviour.
        private = full.name not in extra_public_headers
      headers.append(Header(path=full, private=private, textual=textual))

    return headers


def combine_modulemaps(out: pathlib.Path,
                       modulemaps: list[pathlib.Path],
                       headers: List[Header],
                       module_name: str,
                       extra_modules: list[(str, pathlib.Path)] = []) -> str:
  """Generates the combined modulemap output string from dependencies."""
  custom_header_prefix = os.path.relpath(
      _SRC_PREFIX / 'buildtools/third_party/libc++', out.parent)

  with io.StringIO() as s:
    modules = {'system': 1}
    if module_name:
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
      s.write(_STRIP.sub('', mm_content))
      s.write('\n')

    for header in headers:
      header.path = pathlib.Path(os.path.relpath(header.path, out.parent))
    # Sort by path for determinism.
    headers.sort()

    for header in headers:
      if header.private and header.textual:
        # Private textual headers are semantically equivalent of not being in
        # the modulemap, so we can just ignore them.
        continue
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

    for content, source_modulemap in extra_modules:
      prefix = os.path.relpath(source_modulemap.parent, out.parent)

      def rebase_path(p: str) -> str:
        return os.path.normpath(os.path.join(prefix, p))

      mm_content = _SIMPLE_HEADER_RE.sub(
          lambda m: f'{m.group(1)}{rebase_path(m.group(2))}{m.group(3)}',
          content)
      s.write(mm_content)
      s.write('\n')

    if module_name:
      s.write('}\n')
    return s.getvalue()


def main(args, extra_args):
  """Executes the modulemap generation pipeline."""
  deps = []
  if args.sysroot or args.os == 'win':
    sysroot_dirs = []
    if args.sysroot:
      extra_args.append(f'--sysroot={args.sysroot}')
      subdir = args.sysroot / 'usr/include'
      sysroot_dirs.append(subdir)
      for d in subdir.iterdir():
        # We append *every* triple, not just the correct one.
        # This is because with the target x86_64-unknown-linux-gnu, for example,
        # the directory is really x86_64-linux-gnu, so we can't just do an exact
        # match.
        # This isn't an issue because a file from the wrong triple will never
        # appear in the depfile, which is what this is used for.
        if d.is_dir() and _TRIPLE.match(d.name) is not None:
          sysroot_dirs.append(d)

    if args.os == 'win':
      for arg in extra_args:
        if arg.startswith('/I'):
          sysroot_dirs.append(pathlib.Path(arg.removeprefix('/I')))
    clang_args = [
        str(args.clang),
        *_format_clang_args(
            [
                # Some files are only read with optimization flags enabled.
                '-O2',
                '-D_FORTIFY_SOURCE=3',
                # Ensure we're using the right libc++
                '-nostdinc++',
                f'-I{_SRC_PREFIX}/third_party/libc++/src/include',
                f'-I{_SRC_PREFIX}/third_party/libc++abi/src/include',
                f'-I{_SRC_PREFIX}/buildtools/third_party/libc++',
                # Libc++ feature/hardening macros required by libc++ headers.
                '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE',
                '-D_LIBCPP_BUILDING_LIBRARY',
                '-std=c++23',
                # Ensures that paths to compiler builtin headers are kept
                # relative rather than being resolved to absolute/canonical
                # symlinked paths.
                '-no-canonical-prefixes',
            ],
            args.os),
        *extra_args
    ]

    deps = calculate_transitive_headers(
        clang_args=clang_args,
        include_dirs=[parse_modulemap(mm) for mm in args.modulemap],
        sysroot_dirs=sysroot_dirs,
        extra_public_headers=_HEADERS,
        target_os=args.os,
        target_cpu=args.cpu,
        debug=args.debug,
    )

  known_modules = {}
  for modulemap in args.partial_modulemap:
    for name, content in split_modulemap(modulemap.read_text()).items():
      known_modules[name] = (content, modulemap)

  for module in args.module:
    if module not in known_modules:
      available = ', '.join(sorted(known_modules))
      raise ValueError(f'Module \'{module}\' not found in partial modulemaps. '
                       f'Available: {available}')

  out_str = combine_modulemaps(
      out=args.output,
      modulemaps=args.modulemap,
      headers=deps,
      module_name=args.module_name,
      extra_modules=[known_modules[module] for module in args.module])
  args.output.write_text(out_str)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Generate a system modulemap using clang to discover deps')
  parser.add_argument('--clang',
                      type=pathlib.Path,
                      help='Path to the Clang compiler binary.')
  parser.add_argument('--sysroot',
                      type=pathlib.Path,
                      help='Path to the sysroot directory.')
  parser.add_argument('--output',
                      type=pathlib.Path,
                      required=True,
                      help='Path where the merged modulemap will be written.')
  parser.add_argument('--module-name', help='Name of the module')
  parser.add_argument(
      '--modulemap',
      action='append',
      type=pathlib.Path,
      required=True,
      help='Path to a modulemap to merge. Can be specified multiple times.')
  parser.add_argument(
      '--partial-modulemap',
      action='append',
      type=pathlib.Path,
      default=[],
      help=('Path to a modulemap to partially merge. '
            'Top-level modules must be specified via --module.'))
  parser.add_argument('--module',
                      action='append',
                      type=str,
                      default=[],
                      help=('Name of a top-level module to selectively extract '
                            'from partial modulemaps.'))
  parser.add_argument('--os', help='GN\'s $target_os variable')
  parser.add_argument('--cpu', help='GN\'s $target_cpu variable')
  parser.add_argument(
      '--debug',
      action='store_true',
      help=(
          f'Instead of compiling, generate a bash script {str(_DEBUG_SCRIPT)} '
          'that attempts to compile'))
  args, remaining_args = parser.parse_known_args()
  if remaining_args and remaining_args[0] == '--':
    remaining_args.pop(0)
  main(args, remaining_args)

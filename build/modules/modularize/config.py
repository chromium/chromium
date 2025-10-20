# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import typing

from graph import all_headers
from graph import calculate_rdeps
from graph import Header
from graph import IncludeDir
from graph import Target
from platforms import Os

if typing.TYPE_CHECKING:
  # To fix circular dependency.
  from compiler import Compiler

IGNORED_MODULES = [
    # This is a builtin module with feature requirements.
    'opencl_c',
    # This is a mac module with feature requirements that should be disabled.
    '_stddef',
]

# When any of the following directory names are in the path, it's treated as a
# sysroot directory.
SYSROOT_DIRS = {
    'android_toolchain',
    'debian_bullseye_amd64-sysroot',
    'debian_bullseye_arm64-sysroot',
    'debian_bullseye_armhf-sysroot',
    'debian_bullseye_i386-sysroot',
    'fuchsia-sdk',
    'MacOSX.platform',
    'win_toolchain',
}

# It doesn't matter if these don't work on all platforms.
# It'll just print a warning saying it failed to compile.
# This contains a list of files that aren't depended on by libc++, but we still
# want to precompile.
SYSROOT_PRECOMPILED_HEADERS = [
    'fcntl.h',
    'getopt.h',
    'linux/types.h',
    'sys/ioctl.h',
    'syscall.h',
]


def fix_graph(graph: dict[str, Header],
              compiler: 'Compiler') -> dict[pathlib.Path, str]:
  """Applies manual augmentation of the header graph."""

  def force_textual(key: str):
    if key in graph:
      graph[key].textual = True

  def add_dep(frm, to, check=True):
    if check:
      assert to not in frm.deps
    if to not in frm.deps:
      frm.deps.append(to)

  def skip_module(name):
    found = False
    for hdr in graph.values():
      while True:
        if hdr.root_module == name:
          hdr.textual = True
          found = True
        if hdr.next is None:
          break
        hdr = hdr.next
    assert found

  # We made the assumption that the deps of something we couldn't compile is
  # the intersection of the deps of all users of it.
  # This does not hold true for stddef.h because of __need_size_t
  add_dep(graph['stddef.h'].next, graph['__stddef_size_t.h'], check=False)

  if compiler.os in [Os.Android, Os.Win, Os.Fuchsia]:
    # include_next behaves differently in module builds and non-module builds.
    # Because of this, module builds include libcxx's wchar.h instead of
    # the sysroot's wchar.h
    add_dep(graph['__mbstate_t.h'], graph['wchar.h'])
    # This makes the libcxx/wchar.h included by mbstate_t.h act more like
    # sysroot/wchar.h by preventing it from defining functions.
    graph['__mbstate_t.h'].kwargs['defines'].append(
        '_LIBCPP_WCHAR_H_HAS_CONST_OVERLOADS')
  elif compiler.os.is_apple:
    # This is shadowed by the builtin iso646, so we don't need to build it.
    graph['iso646.h'].next.textual = True

  rdeps = calculate_rdeps(all_headers(graph))

  sysroot = graph['assert.h'].abs.parent
  for header in all_headers(graph):
    header.direct_deps = header.calculate_direct_deps(graph, sysroot=sysroot)

  if compiler.os.is_apple:
    # See https://github.com/llvm/llvm-project/issues/154675
    # Darwin defines the symbol "echo" in curses.h
    # Although curses.h is not included, the symbol is part of the module and
    # thus we get an error when attempting to use the symbol "echo" after
    # including *any* part of the module Darwin.
    skip_module("Darwin")
    # This module isn't intended to be used - it's intended to catch
    # misconfigured sysroots.
    skip_module("_c_standard_library_obsolete")
  else:
    for header in all_headers(graph):
      if header.include_dir != IncludeDir.Sysroot:
        continue

      parts = set(pathlib.Path(header.rel).parts)
      # We want non-textual, but we don't need to do so if the header including
      # you via include_next is non-textual.
      if header.prev is not None:
        header.textual = not header.prev.textual
      # Anything not to be included by the user directly that was only included
      # once can be marked as textual. Unfortunately since .d files calculate
      # *transitive* dependencies this is not particularly effective.
      elif (len(rdeps[header]) < 2
            and parts.intersection(['asm', 'asm-generic', 'bits'])):
        header.textual = True
      elif '#pragma once' in (header.content or ''):
        header.textual = False
      elif 'bits' in parts:
        header.textual = True

    # Assert is inherently textual.
    graph['assert.h'].textual = True

  force_textual('asm-generic/unistd.h')
  force_textual('asm-generic/bitsperlong.h')

  if compiler.os == Os.Android:
    graph['android/legacy_stdlib_inlines.h'].textual = True
    graph['android/legacy_threads_inlines.h'].textual = True
    graph['android/legacy_unistd_inlines.h'].textual = True
    graph['bits/threads_inlines.h'].textual = True

    graph['asm-generic/posix_types.h'].textual = True
    graph['asm/posix_types.h'].textual = True

    # sys/syscall.h includes asm/unistd.h, which includes
    # asm/unistd_<platform>.h, which defines some macros.
    # It then includes bits/glibc-syscalls.h which uses said macros, so both
    # must be non-textual.
    for k in graph:
      if k.startswith('asm/unistd'):
        graph[k].textual = True
    graph['bits/glibc-syscalls.h'].textual = True

  elif compiler.os == Os.Linux:
    # See https://codebrowser.dev/glibc/glibc/sysdeps/unix/sysv/linux/bits/local_lim.h.html#56
    # if linux/limits.h is non-textual, then limits.h undefs the limits.h
    # defined in the linux/limits.h module.
    # Thus, limits.h exports an undef.
    # if it's textual, limits.h undefs something it defined itself.
    graph['linux/limits.h'].textual = True

    # This is not included on arm32
    graph['asm-generic/types.h'].textual = True

    # On chromeos, x86_64-linux-gnu/foo.h will be either moved to foo.h or to
    # x86_64-cros-gnu.
    # So we just mark them all as textual so they don't appear in the modulemap.
    for hdr in graph.values():
      if '-linux-gnu' in str(hdr.abs):
        hdr.textual = True

  # Windows has multiple include directories contained with the sysroot.
  if compiler.os == Os.Win:
    graph['math.h'].kwargs['defines'].append('_USE_MATH_DEFINES')
    return {
        graph['corecrt.h'].abs.parent.parent: '$windows_kits',
        graph['eh.h'].abs.parent: '$msvc',
    }
  else:
    return {sysroot: '$sysroot'}


def should_compile(target: Target) -> bool:
  """Decides whether a target should be compiled or not.

  If this returns true, the target should be compiled.
  If this returns false, the target *may* be compiled (eg. if a target that
    should be compiled depends on this).
  """
  for header in target.headers:
    # For now, we only precompile the transitive dependencies of libcxx, and
    # nothing else in the sysroot.
    if header.include_dir == IncludeDir.LibCxx:
      return True

  return False

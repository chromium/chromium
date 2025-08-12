# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import pathlib

from compiler import Compiler
from graph import all_headers
from graph import CompileStatus
from graph import Header
from graph import IncludeDir
from graph import calculate_rdeps

IGNORED_MODULES = [
    # This is a builtin module with feature requirements.
    'opencl_c',
    # This is a mac module with feature requirements that should be disabled.
    '_stddef',
]

# When any of the following directory names are in the path, it's treated as a toolchain directory.
SYSROOT_DIRS = {
    'android_toolchain',
    'debian_bullseye_amd64-sysroot',
    'MacOSX.platform',
    'win_toolchain',
}

# It doesn't matter if these don't work on all platforms.
# It'll just print a warning saying it failed to compile.
# This contains a list of files that aren't depended on by libc++, but we still
# want to precompile.
SYSROOT_PRECOMPILED_HEADERS = [
    'fcntl.h',
]


def fix_graph(graph: dict[str, Header], compiler: Compiler):
  """Applies manual augmentation of the header graph."""

  def add_dep(frm, to):
    assert to not in frm.deps
    frm.deps.append(to)

  # We made the assumption that the deps of something we couldn't compile is
  # the intersection of the deps of all users of it.
  # This does not hold true for stddef.h because of __need_size_t
  add_dep(graph['stddef.h'].next, graph['__stddef_size_t.h'])

  rdeps = calculate_rdeps(all_headers(graph))

  sysroot = graph['assert.h'].abs.parent
  for header in all_headers(graph):
    header.direct_deps = header.calculate_direct_deps(graph, sysroot=sysroot)

  if compiler.is_apple:
    # From here on out we're modifying which headers are textual.
    # This isn't relevant to apple since it has a modulemap.
    return

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

  # This is included from the std_wchar_h module, but that module is marked as
  # textual. Normally that would mean we would mark this as non-textual, but
  # wchar.h doesn't play nice being non-textual.
  graph['wchar.h'].next.textual = True

  if compiler.os == 'android':
    graph['wchar.h'].public_configs.append(
        '//buildtools/third_party/libc++:wchar_android_fix')

    graph['android/legacy_threads_inlines.h'].textual = True
    graph['bits/threads_inlines.h'].textual = True

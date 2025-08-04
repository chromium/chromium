# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import pathlib

from graph import IncludeDir
from graph import Header
from graph import HeaderRef


def fix_graph(graph: dict[HeaderRef, Header], os: str, cpu: str):
  """Applies manual augmentation of the header graph."""
  is_apple = os in ['mac', 'ios']

  # Deal with include_next for modules with modulemaps.
  # We were only able to compile the one in the first include dir.
  # To solve this, we copy all dependencies except the shadow to the shadowed
  # header file.
  # eg. If libcxx/stddef.h has deps [builtin/stddef.h, sysroot/stdint.h], we
  # set the deps of builtin/stddef.h to [sysroot/stdint.h]
  includes = {}
  for rel in {header.rel for header in graph.values()}:
    order = []
    for d in IncludeDir:
      header = graph.get((d, rel), None)
      if header is not None:
        order.append(header)
    for prev, header in zip(order, order[1:]):
      header.deps = [(to_kind, to_rel) for (to_kind, to_rel) in order[0].deps
                     if to_rel != header.rel or to_kind > header.include_dir]
      header.prev = prev
      prev.next = header
    includes[rel] = order

  for header in graph.values():
    header.direct_deps = header.calculate_direct_deps(includes)

  if is_apple:
    # From here on out we're modifying which headers are textual.
    # This isn't relevant to apple since it has a modulemap.
    return

  # Calculate a reverse dependency graph
  rdeps = collections.defaultdict(list)
  for header in graph.values():
    for dep in header.deps:
      rdeps[graph[dep]].append(header)

  sysroot = lambda rel, kind=IncludeDir.Sysroot: graph[(kind, rel)]

  for header in graph.values():
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
  sysroot('assert.h').textual = True

  # This is included from the std_wchar_h module, but that module is marked as
  # textual. Normally that would mean we would mark this as non-textual, but
  # wchar.h doesn't play nice being non-textual.
  sysroot('wchar.h').textual = True

  if os == 'android':
    graph[(IncludeDir.LibCxx, 'wchar.h')].public_configs.append(
        '//buildtools/third_party/libc++:wchar_android_fix')

    sysroot('android/legacy_threads_inlines.h').textual = True
    sysroot('bits/threads_inlines.h').textual = True

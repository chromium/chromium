# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import collections
import dataclasses
import enum
import functools
import itertools
import pathlib
import re

_INCLUDES = re.compile(r'#\s*include(_next)?\s*<([^>]+)>')


class IncludeDir(enum.Enum):
  # Ordered by include order for clang
  LibCxx = 1
  Builtin = 2
  DarwinBasic = 3
  DarwinFoundation = 4
  CStandardLibrary = 5
  Sysroot = 6

  def __lt__(self, other):
    return self.value < other.value


HeaderRef = tuple[IncludeDir, str]


@dataclasses.dataclass
class Header:
  include_dir: IncludeDir
  # The string to use in your #include statement to get this file.
  rel: str
  # The absolute path to the file
  # This should be filled by the time the compiler finishes.
  abs: pathlib.Path = None
  # Prev and next come from include_next / include_prev
  prev: None | Header = None
  next: None | Header = None
  root_module: None | str = None
  textual: bool = False
  deps: list[HeaderRef] = dataclasses.field(default_factory=list)
  direct_deps: set[Header] = dataclasses.field(default_factory=set)
  # Here, None means no exports, and the empty list means 'export *'
  # We default to exporting all to preserve the behaviour of includes.
  exports: None | list[HeaderRef] = dataclasses.field(default_factory=list)

  # Any configs required to build this file.
  public_configs: list[str] = dataclasses.field(default_factory=list)

  def __hash__(self):
    return hash((self.include_dir, self.rel))

  def __eq__(self, other):
    return (self.include_dir, self.rel) == (other.include_dir, other.rel)

  def __lt__(self, other):
    return (self.include_dir, self.rel) < (other.include_dir, other.rel)

  @property
  def pretty_name(self):
    return f'{self.include_dir.name}/{self.rel}'

  def __repr__(self):
    return self.pretty_name

  @property
  def submodule_name(self):
    normalized = self.rel.replace('.', '_').replace('/', '_').replace('-', '_')
    return 'sysroot_' + normalized

  @functools.cached_property
  def content(self) -> None | str:
    return self.abs.read_text()

  def calculate_direct_deps(self, includes: dict[str,
                                                 list[Header]]) -> set[Header]:
    direct = set()
    for is_next, include in _INCLUDES.findall(self.content):
      header = None
      order = includes.get(include, [])
      if is_next and self in order[:-1]:
        header = order[order.index(self) + 1]
      if not is_next and order:
        header = order[0]

      # It might have been conditionally included.
      if header is not None and (header.include_dir, header.rel) in self.deps:
        direct.add(header)
    return direct

  def direct_deps_closure(self) -> set[Header]:
    closure = set(self.direct_deps)
    for dep in self.direct_deps:
      if dep.textual:
        closure.remove(dep)
        closure.update(dep.direct_deps_closure())
    return closure


@dataclasses.dataclass
class Target:
  """Represents a single clang module / gn target."""
  include_dir: IncludeDir
  name: str
  headers: list[Header] = dataclasses.field(default_factory=list)

  def __lt__(self, other):
    return self.name < other.name


def run_build(graph: dict[HeaderRef, Header]) -> list[Target]:
  """Calculates the correct way to run a build."""
  unbuilt_modules: dict[str, list[Header]] = collections.defaultdict(list)
  unbuilt_headers: set[Header] = set()

  for header in graph.values():
    if not header.textual:
      if header.root_module is None:
        unbuilt_headers.add(header)
      else:
        unbuilt_modules[header.root_module].append(header)
    header.rdeps = set()
    # A group is a set of headers that must be compiled together, because they
    # form a dependency loop.
    header.group = [header]
    header.mod_deps = set()
    header.unbuilt_deps = set([
        graph[dep] for dep in header.deps
        # Textual headers don't need to be built.
        # Also, you don't need to wait for a dependency within the same module.
        if not graph[dep].textual and \
          (header.root_module is None or \
           header.root_module != graph[dep].root_module)
    ])

  for header in graph.values():
    for dep in header.unbuilt_deps:
      dep.rdeps.add(header)

  # Break dependency loops.
  for header in sorted(graph.values()):
    for dep in list(header.unbuilt_deps):
      # Mark all headers but one in the loop as already having been built.
      if header in dep.unbuilt_deps and header.include_dir == IncludeDir.Sysroot and dep.include_dir == IncludeDir.Sysroot:
        header.group.append(dep)
        dep.group = header.group
        for rdep in dep.rdeps:
          assert header is rdep or header in rdep.unbuilt_deps
          rdep.unbuilt_deps.remove(dep)
        unbuilt_headers.remove(dep)

  build_gn = []

  for i in itertools.count():
    # Try and build any buildable modules from modulemaps.
    while True:
      n_remaining = len(unbuilt_modules)
      for mod, headers in list(unbuilt_modules.items()):
        if not any(header.unbuilt_deps for header in headers):
          build_gn.append(
              Target(
                  include_dir=headers[0].include_dir,
                  name=mod,
                  headers=sorted(headers),
              ))
          del unbuilt_modules[mod]
          for header in headers:
            for rdep in header.rdeps:
              rdep.mod_deps.add(header.root_module)
              rdep.unbuilt_deps.remove(header)

      if n_remaining == len(unbuilt_modules):
        break

    # Try and build any builtable sysroot modules
    sysroot_mod = f'sys_stage{i + 1}'
    build_gn.append(Target(
        include_dir=IncludeDir.Sysroot,
        name=sysroot_mod,
    ))
    while True:
      n_remaining = len(unbuilt_headers)
      for header in list(unbuilt_headers):
        if not header.unbuilt_deps:
          header.root_module = sysroot_mod
          build_gn[-1].headers.append(header)
          unbuilt_headers.remove(header)
          for rdep in header.rdeps:
            rdep.mod_deps.add(header.root_module)
            rdep.unbuilt_deps.remove(header)

      if n_remaining == len(unbuilt_headers):
        break

    build_gn[-1].headers.sort()
    if not build_gn[-1].headers:
      break

  build_gn.pop()

  if not unbuilt_modules and not unbuilt_headers:
    # Success. Everything is built
    return build_gn
  else:
    print(
        "Dependency loop in sysroot. You probably want to make one of them textual."
    )
    print("The following headers are in a dependency loop:")
    pairs = set()
    for header in unbuilt_headers:
      for dep in header.unbuilt_deps:
        if header in dep.unbuilt_deps:
          print(f'{header.pretty_name} -> {dep.pretty_name}')

    # If you get to this point, you probably want a debugger to help understand what the problem is.
    sysroot = lambda rel: graph[(IncludeDir.Sysroot, rel)]
    breakpoint()
    exit(1)

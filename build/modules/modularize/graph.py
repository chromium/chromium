# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import collections
import contextlib
import dataclasses
import enum
import functools
import itertools
import pathlib
import re

# Almost every sysroot just uses #include <>, but fuchsia uses #include ""
# sometimes.
_INCLUDES = re.compile(r'#\s*(?:include|import)(_next)?\s*["<]([^>"]+)[>"]')


class CompileStatus(enum.Enum):
  NotCompiled = 1
  Success = 2
  Failure = 3


class IncludeDir(enum.Enum):
  # Ordered by include order for clang
  LibCxx = 1
  Builtin = 2
  SysrootModule = 3
  Sysroot = 4
  Framework = 5

  def __lt__(self, other):
    return self.value < other.value


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
  umbrella: bool = False
  compile_status: CompileStatus = CompileStatus.NotCompiled

  deps: list[Header] = dataclasses.field(default_factory=list)
  direct_deps: set[Header] = dataclasses.field(default_factory=set)
  # Here, None means no exports, and the empty list means 'export *'
  # We default to exporting all to preserve the behaviour of includes.
  exports: None | list[str] = dataclasses.field(default_factory=list)

  # Kwargs that will end up on the BUILD.gn targets.
  kwargs: dict[str, list[str]] = dataclasses.field(
      default_factory=lambda: collections.defaultdict(list))

  def __hash__(self):
    return hash((self.include_dir, self.rel))

  def __eq__(self, other):
    if isinstance(other, Header):
      return (self.include_dir, self.rel) == (other.include_dir, other.rel)
    else:
      # This allows you to write (Sysroot, 'foo.h') in set[Header]
      return (self.include_dir, self.rel) == other

  def __lt__(self, other):
    return (self.include_dir, self.rel) < (other.include_dir, other.rel)

  @property
  def pretty_name(self):
    return f'{self.include_dir.name}/{self.rel}'

  def __repr__(self):
    return self.pretty_name

  @property
  def submodule_name(self):
    return self.rel.replace('.', '_').replace('/', '_').replace('-', '_')

  @functools.cached_property
  def content(self) -> str:
    return self.abs.read_text(errors='ignore')

  def calculate_direct_deps(self, includes: dict[str, Header],
                            sysroot: pathlib.Path) -> set[Header]:

    direct = set()
    found_includes = _INCLUDES.findall(self.content)

    def find_include(is_next, include) -> bool:
      header = None
      first = includes.get(include, None)
      if first is not None:
        # When modules are enabled, #include_next<foo.h> from any file other
        # than foo.h is treated the same as #include <foo.h>.
        if not is_next or (is_next and self.rel != include):
          header = first
        elif self.next is not None:
          header = self.next

        # It might have been conditionally included.
        if header is not None and header in self.deps:
          direct.add(header)
          return True
      return False

    for is_next, include in found_includes:
      if not find_include(is_next, include):
        # This is required, because, for example, libcxx's threading includes
        # pthread.h, but the include scanner sees pthread/pthread.h (the
        # symlink target).
        with contextlib.suppress(OSError, FileNotFoundError):
          find_include(is_next, str((sysroot / include).readlink()))

    return direct

  @functools.cache
  def _required_deps(self) -> tuple[set[Header], set[Header]]:
    nontextual = set()
    textual = set()
    todo = [self]
    while todo:
      hdr = todo.pop()
      for dep in hdr.direct_deps:
        if dep.textual and dep not in textual:
          todo.append(dep)
          textual.add(dep)
        elif not dep.textual:
          nontextual.add(dep)
    return nontextual, textual

  @property
  def required_deps(self) -> set[Header]:
    """The header files required to be built before we can build."""
    return self._required_deps()[0]

  @property
  def required_textual_deps(self) -> set[Header]:
    """The textual header files we directly include.

    This includes textual headers included via other textual headers"""
    return self._required_deps()[1]

  def find_loop(self) -> list[Header] | None:
    """Finds a loop of #includes, if it exists."""
    chain = [self]
    has_chain = True
    while has_chain:
      has_chain = False
      if self in chain[-1].direct_deps:
        return chain + [self]
      for dep in chain[-1].direct_deps:
        if dep not in chain and self in dep.deps:
          chain.append(dep)
          has_chain = True
          break
    # It shouldn't be possible to have a node that has you as a transitive
    # dep without having a dep that has you as a transitive dep.
    assert len(chain) == 1


def calculate_rdeps(headers: list[Header]) -> dict[Header, list[Header]]:
  """Calculates a reverse dependency graph"""
  rdeps = collections.defaultdict(list)
  for header in headers:
    for dep in header.deps:
      rdeps[dep].append(header)
  return rdeps


def all_headers(graph: dict[str, Header]):
  """Iterates through all headers in a graph."""
  for header in graph.values():
    while header is not None:
      yield header
      header = header.next


@dataclasses.dataclass
class Target:
  """Represents a single clang module / gn target."""
  include_dir: IncludeDir
  name: str
  headers: list[Header] = dataclasses.field(default_factory=list)

  def __lt__(self, other):
    return self.name < other.name

  def __eq__(self, other):
    return self.name == other.name

  def __hash__(self):
    return hash(self.name)

  @property
  def kwargs(self) -> dict[str, set[str]]:
    """The kwargs associated with a build target.

    eg. if you have kwargs = {"defines": ["FOO"]}, then it outputs:

    target_type(target.name) {
      defines = ["FOO"]
    }
    """
    kwargs = collections.defaultdict(set)
    for header in self.headers:
      for single in header.group:
        for dep in {single} | single.required_textual_deps:
          for k, v in dep.kwargs.items():
            kwargs[k].update(v)
    return kwargs

  @property
  def header_deps(self) -> set[Header]:
    direct_deps = set()
    for hdr in self.headers:
      direct_deps.update(hdr.required_deps)
    return direct_deps

  @property
  def public_deps(self) -> list[str]:
    return sorted(
        set([
            hdr.root_module for hdr in self.header_deps
            if hdr.root_module is not None and hdr.root_module != self.name
        ]))


def run_build(graph: dict[str, Header]) -> list[Target]:
  """Calculates the correct way to run a build."""
  unbuilt_modules: dict[str, list[Header]] = collections.defaultdict(list)
  unbuilt_headers: set[Header] = set()

  for header in all_headers(graph):
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
    header.unbuilt_deps = set(
        dep for dep in header.required_deps
        # You don't need to wait for a dependency within the same module.
        if (header.root_module is None or header.root_module != dep.root_module)
        and dep != header)

  # Perform a union find to find all dependency loops.
  # Since we can easily tell if a given edge represents a dependency loop, we
  # simply union together all pairs of nodes on loop edges.
  # We perform a simple form of union find where we don't bother with rank or
  # size, and only do path compression (always on the lexicographically first
  # header). It's slower but still plenty fast and simplifies things.
  parents = {}

  def find(header):
    if header in parents:
      # Optimization: path compression
      parents[header] = find(parents[header])
      return parents[header]
    else:
      return header

  for header in sorted(unbuilt_headers):
    for dep in header.required_deps:
      if dep > header and header in dep.deps:
        assert header.include_dir == IncludeDir.Sysroot and dep.include_dir == IncludeDir.Sysroot, (
            header, dep)
        # Perform the 'union' operation.
        x, y = sorted([find(header), find(dep)])
        if x != y:
          parents[y] = x

  loops = collections.defaultdict(list)
  for header in unbuilt_headers:
    loops[find(header)].append(header)

  for headers in loops.values():
    # Not a loop
    if len(headers) == 1:
      continue
    headers.sort()
    headers[0].group = headers
    headers[0].unbuilt_deps = set.union(
        *[header.unbuilt_deps for header in headers]) - set(headers)
    for header in headers[1:]:
      unbuilt_headers.remove(header)

  for header in all_headers(graph):
    for dep in header.unbuilt_deps:
      dep.rdeps.add(header)

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
          build_gn[-1].headers.append(header)
          unbuilt_headers.remove(header)
          for header in header.group:
            header.root_module = sysroot_mod
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
    seen = set()
    for header in unbuilt_headers:
      if header not in seen:
        chain = header.find_loop()
        if chain is not None:
          print(' -> '.join([header.pretty_name for header in chain]))
          seen.update(chain)

    # If you get to this point, you probably want a debugger to help understand what the problem is.
    breakpoint()
    exit(1)

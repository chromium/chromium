# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import pathlib
import re

from graph import Header
from graph import IncludeDir

_MODULE_START = re.compile(
    r'^(\s*)(?:explicit )?((?:framework )?)module ([^\s.{]+)', flags=re.I)
_HEADER = re.compile(
    r'\s+(exclude header|textual header|umbrella header|header|umbrella) "([^"]*)"'
)
_EXTERN_MODULE = re.compile(r'^(\s*)extern module ([^ ]*) "([^"]*)"')


def _parse_modulemap(include_dir: pathlib.Path, mm_path: pathlib.Path,
                     include_kind: IncludeDir,
                     modules: collections.defaultdict[str, list],
                     headers: set[Header]):
  # The modulemap's paths are relative to the modulemap, but the include's
  # paths are relative to d.
  rel = str(mm_path.parent.relative_to(include_dir))
  rel_prefix = '' if rel == '.' else f'{rel}/'
  framework_layers = []

  def absolute(rel: str, check_exist=True):
    # We don't have / support three layers of nesting of frameworks.
    assert len(framework_layers) < 3
    if include_kind != IncludeDir.Framework:
      path = mm_path.parent / rel
    # Frameworks have very specific path requirements.
    elif len(framework_layers) == 1:
      path = include_dir / 'Headers' / rel
    elif len(framework_layers) == 2:
      path = include_dir / f'Frameworks/{framework_layers[1]}.framework/Headers' / rel
    if check_exist:
      assert path.is_file(), path
    return path

  def relative(rel: str):
    if include_kind == IncludeDir.Framework:
      # In the event of foo.framework/Frameworks/bar.framework/..., it's bar
      return f'{framework_layers[-1]}/{rel}'
    else:
      return rel_prefix + rel

  with mm_path.open() as f:
    for line in f:
      match = _MODULE_START.match(line)
      if match is not None:
        indent, is_framework, mod = match.groups()
        if not indent:
          # It must be a root module
          current_module = mod
          if mm_path not in modules[current_module]:
            modules[current_module].append(mm_path)
        if is_framework:
          # This is a bit hacky, but frameworks don't go deeper than two layers.
          if not indent:
            framework_layers = [mod]
          else:
            framework_layers = [framework_layers[0], mod]

      header = _HEADER.search(line)
      if header is not None:
        kind, name = header.groups()
        if kind in ['header', 'umbrella header', 'textual header']:
          headers.add(
              Header(
                  root_module=current_module,
                  include_dir=include_kind,
                  rel=relative(name),
                  abs=absolute(name),
                  textual=kind == 'textual header',
                  umbrella=kind == 'umbrella header',
              ))
        if kind == 'umbrella':
          if len(framework_layers) == 2:
            # In case of submodules, reroot at the submodule
            # a is an arbitrary filename that we discard with parents.
            mod_root = absolute('a', must_exist=False).parents[1]
          elif include_kind == IncludeDir.Framework:
            mod_root = include_dir
          else:
            mod_root = mm_path.parent
          for path in mod_root.glob(f"{name}/*.h"):
            # We need a way to calculate what the name *should* have been.
            rel = str(
                path.relative_to(mod_root)).removeprefix('Headers').lstrip('/')
            headers.add(
                Header(
                    root_module=current_module,
                    include_dir=include_kind,
                    rel=relative(rel),
                    abs=path,
                    textual=False,
                ))
      extern = _EXTERN_MODULE.match(line)
      if extern is not None:
        indent, extern_module_name, modulemap = extern.groups()
        paths = modules[current_module if indent else extern_module_name]
        if mm_path not in paths:
          paths.append(mm_path)
        submap_headers = set()
        _parse_modulemap(include_dir,
                         include_dir / modulemap,
                         include_kind,
                         modules=modules,
                         headers=submap_headers)
        # For module foo { extern module bar }, although the module is bar, the
        # compilation unit is foo
        if indent:
          for hdr in submap_headers:
            hdr.root_module = current_module
        headers.update(submap_headers)


def calculate_modules(
    include_kinds: list[tuple[pathlib.Path, IncludeDir]]
) -> tuple[dict[str, list[pathlib.Path]], set[Header]]:
  """Calculates modules and the headers contained within.

  Args:
    include_kinds: A list of include dirs

  Returns:
    A mapping from module names to modulemaps, and headers defined by modulemaps
  """
  modules = collections.defaultdict(list)
  headers = set()
  for d, kind in include_kinds:
    if kind == IncludeDir.Framework:
      # For the semantics of frameworks, see
      # https://clang.llvm.org/docs/Modules.html#module-declaration
      for modulemap in d.glob("**/Modules/module.modulemap"):
        if 'Versions' not in modulemap.parts:
          _parse_modulemap(modulemap.parents[1],
                           modulemap,
                           include_kind=kind,
                           modules=modules,
                           headers=headers)
    else:
      if (d / 'module.modulemap').is_file():
        _parse_modulemap(d,
                         d / 'module.modulemap',
                         include_kind=kind,
                         modules=modules,
                         headers=headers)
      # One level deep is sufficient for the apple sysroot.
      # ** doesn't work because otherwise it includes the things referenced by
      # the root module.modulemap
      for modulemap in d.glob('*/module.modulemap'):
        _parse_modulemap(d,
                         modulemap,
                         include_kind=kind,
                         modules=modules,
                         headers=headers)

  return modules, headers

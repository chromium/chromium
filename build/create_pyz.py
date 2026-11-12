#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a .pyz archive of precompiled .pyc files.

Archive layout:
  __main__.pyc          Entry point executed by `python3 foo.pyz`.
  _pyz_mp_main.pyc      Bootstrap + sys.path_hooks importer; also imported as
                        `__mp_main__` by `multiprocessing` 'spawn' workers.
  _pyz/<rel_path>.pyc   Bundled bytecode mirroring checkout-relative paths
                        under `//` (nested under `_pyz/` so keeping `foo.pyz`
                        on `sys.path` only exposes `_pyz_mp_main` at root).

Why a custom bootstrap is needed instead of compiling the main script directly
as `__main__.pyc` and relying on standard `zipimport`:
  1. `__file__` fidelity: Chromium scripts (and imported modules such as Mojo's
     `template_expander.py`) compute sibling source/data paths via
     `os.path.dirname(__file__)`. Standard `zipimport` points `__file__` inside
     `out/.../foo.pyz` rather than the checkout tree `//...`.
  2. Runtime `sys.path` edits: Scripts add checkout directories (`//build`,
     `//third_party/...`, etc.) to `sys.path` at runtime. Standard `zipimport`
     only services `foo.pyz` on `sys.path`, not filesystem paths under `//`.
  3. Hermeticity: For directories present in `_index`, only explicitly declared
     `.pydeps`/`inputs` modules are importable, so missing dependency entries
     fail locally instead of silently reading unbundled `.py` files off disk
     and failing only on RBE.
  4. `multiprocessing` 'spawn' (default on Windows, used by `mojom_parser.py`):
     CPython runs a zip's `__main__.pyc` with `__spec__.name == '__main__'`,
     which `multiprocessing.spawn` explicitly skips in child workers
     (`_fixup_main_from_name`). Setting `__spec__ = None` (`init_main_from_path`
     via `__file__`) also fails on RBE where `.py` sources are absent on disk.
     Setting `__spec__.name = '_pyz_mp_main'` and keeping `foo.pyz` on
     `sys.path` lets fresh child workers import `_pyz_mp_main.pyc` via built-in
     `zipimporter`, install `_hook`, and load the entry script as `__mp_main__`.
"""

import argparse
import importlib.util
import marshal
import os
import posixpath
import sys
import zipfile

import action_helpers
import zip_helpers

_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))

# PEP 552 unchecked hash-based .pyc flags (bit 0 = hash-based, bit 1 = 0).
_UNCHECKED_HASH_FLAGS = b'\x01\x00\x00\x00'

# Entry kinds stored in _index[dir_rel][mod_name]:
_KIND_MODULE = 0
_KIND_PACKAGE = 1
_KIND_NAMESPACE = 2

_MP_MAIN_TEMPLATE = """\
# Wrap bootstrap helpers in a closure so they survive `g.clear()` when `_run()`
# replaces the target module dict (`__main__` or `__mp_main__`) in place.
def _init():
    # Use already-loaded frozen bootstrap modules instead of `importlib.util` /
    # `importlib.machinery` to avoid extra stdlib imports on every invocation.
    import _frozen_importlib as _fi
    import _frozen_importlib_external as _fie
    import os
    import sys
    import zipimport

    _archive = __loader__.archive
    _archive_abs = os.path.abspath(_archive)
    _archive_dir = os.path.dirname(_archive_abs)
    # Resolve `//` relative to the `.pyz` location so the archive stays
    # relocatable across local output directories and RBE workers.
    _src_root = os.path.normpath(
        os.path.join(_archive_dir, {src_root_rel!r})
    )
    # Maps checkout-relative POSIX dir -> {{module_or_pkg_name: kind}}.
    _index = {index!r}

    # Returns a zipimporter for the given checkout-relative POSIX dir, which
    # shares the archive's cached central directory with `__loader__`. It is
    # used as the loader for bundled modules: its get_code() validates the
    # .pyc header, while `__file__` comes from the spec's checkout `origin`.
    def _zipimporter(rel):
        path = os.path.join(_archive, '_pyz')
        if rel:
            path = os.path.join(path, rel)
        return zipimport.zipimporter(path)

    class _Finder:
        def __init__(self, dir_path, rel, entries):
            self._dir_path = dir_path
            self._zimporter = _zipimporter(rel)
            self._entries = entries

        def find_spec(self, fullname, target=None):
            mod = fullname.rpartition('.')[2]
            kind = self._entries.get(mod)
            if kind is None:
                # Undeclared module in an indexed checkout directory: block
                # fallback to disk so missing `.pydeps` fail deterministically.
                return None
            sub_path = os.path.join(self._dir_path, mod)
            if kind == {kind_namespace}:
                # PEP 420 implicit namespace package (directory without
                # `__init__.py` containing bundled submodules).
                spec = _fi.ModuleSpec(fullname, None)
                spec.submodule_search_locations = [sub_path]
                return spec
            if kind == {kind_module}:
                origin = sub_path + '.py'
                search_locations = None
            else:
                # Regular package (`__init__.py`): set search_locations to the
                # checkout path so submodule imports re-enter `_hook`.
                origin = os.path.join(sub_path, '__init__.py')
                search_locations = [sub_path]
            return _fie.spec_from_file_location(
                fullname,
                origin,
                loader=self._zimporter,
                submodule_search_locations=search_locations,
            )

    _nc_src_root = os.path.normcase(_src_root)
    _src_prefix = os.path.join(_src_root, '')
    _nc_src_prefix = os.path.normcase(_src_prefix)

    # `sys.path_hooks` factory: intercepts `sys.path` entries under `_src_root`
    # that appear in `_index`, and raises `ImportError` for everything else
    # (stdlib, build dir, non-Python dirs) so Python's normal finders handle it.
    def _hook(p):
        ap = os.getcwd() if not p else os.path.abspath(p)
        nc = os.path.normcase(ap)
        if nc == _nc_src_root:
            rel = ''
            dir_path = _src_root
        elif nc.startswith(_nc_src_prefix):
            suffix = ap[len(_src_prefix) :]
            rel = suffix.replace(os.sep, '/')
            dir_path = _src_prefix + suffix
        else:
            raise ImportError
        entries = _index.get(rel)
        if entries is None:
            raise ImportError
        return _Finder(dir_path, rel, entries)

    _entry_rel = {entry_rel!r}
    _entry_file = os.path.join(_src_root, _entry_rel.replace('/', os.sep))
    _entry_dir_rel, _, _entry_name = _entry_rel[:-3].rpartition('/')
    _entry_loader = _zipimporter(_entry_dir_rel)

    def _run(is_main, g, _builtins=__builtins__, _exec=exec):
        sys.path_hooks.insert(0, _hook)
        sys.path_importer_cache.clear()
        if is_main:
            # Match `python3 path/to/script.py`: put the script's directory at
            # `sys.path[0]`, and keep the `.pyz` at the end of `sys.path` so
            # spawned `multiprocessing` workers can import `_pyz_mp_main`.
            sys.path[0] = os.path.dirname(_entry_file)
            if _archive_abs not in sys.path:
                sys.path.append(_archive_abs)
            run_name = '__main__'
        else:
            run_name = '__mp_main__'
        # Advertise `_pyz_mp_main` as `__spec__.name` (not `'__main__'`) so
        # `multiprocessing.spawn` sets `init_main_from_name = '_pyz_mp_main'`
        # in child (and grandchild) worker processes.
        spec = _fi.ModuleSpec(
            '_pyz_mp_main', _entry_loader, origin=_entry_file
        )
        code = _entry_loader.get_code(_entry_name)
        saved = {{
            '__name__': run_name,
            '__doc__': None,
            '__file__': _entry_file,
            '__cached__': None,
            '__package__': None,
            '__loader__': _entry_loader,
            '__spec__': spec,
            '__builtins__': _builtins,
        }}
        g.clear()
        g.update(saved)
        _exec(code, g)

    return _run


_run = _init()
# Executed in `multiprocessing` 'spawn' child processes when `spawn.py` calls
# `runpy.run_module('_pyz_mp_main', run_name='__mp_main__')`.
if __name__ == '__mp_main__':
    _run(False, globals())
"""

# `__main__.pyc` is invoked by CPython when running `python3 foo.pyz`; it
# delegates to `_pyz_mp_main` so normal startup and `multiprocessing` 'spawn'
# share the same bootstrap.
_MAIN_SRC = b'import _pyz_mp_main\n_pyz_mp_main._run(True, globals())\n'


def _compile_pyc(source_bytes, display_path):
    """Compiles `source_bytes` into a PEP 552 unchecked-hash .pyc."""
    code = compile(source_bytes, display_path, 'exec', dont_inherit=True)
    return (
        importlib.util.MAGIC_NUMBER
        + _UNCHECKED_HASH_FLAGS
        + importlib.util.source_hash(source_bytes)
        + marshal.dumps(code)
    )


def _read_pydeps(pydeps_path):
    base_dir = os.path.dirname(pydeps_path)
    paths = []
    with open(pydeps_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('//'):
                paths.append(
                    os.path.normpath(os.path.join(_SRC_ROOT, line[2:]))
                )
            else:
                paths.append(os.path.normpath(os.path.join(base_dir, line)))
    return paths


def _to_src_rel_posix(path):
    rel = os.path.relpath(path, _SRC_ROOT)
    rel_posix = rel.replace(os.sep, '/')
    if rel_posix == '..' or rel_posix.startswith('../'):
        raise ValueError(
            f'Python source {path!r} is outside source root {_SRC_ROOT!r}'
        )
    if not rel_posix.endswith('.py'):
        raise ValueError(f'Expected a .py file, got: {path!r}')
    return rel_posix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, help='Output .pyz path.')
    parser.add_argument(
        '--script', required=True, help='Entry-point .py script path.'
    )
    parser.add_argument('--pydeps', help='Path to .pydeps file.')
    parser.add_argument(
        '--inputs',
        action='append',
        help='GN list of additional .py files to bundle.',
    )
    action_helpers.add_depfile_arg(parser)
    options = parser.parse_args()

    input_paths = []
    if options.pydeps:
        input_paths.extend(_read_pydeps(options.pydeps))
    if options.inputs:
        input_paths.extend(action_helpers.parse_gn_list(options.inputs))
    input_paths.append(options.script)

    entry_rel = _to_src_rel_posix(options.script)
    sources_by_rel = {}
    for p in input_paths:
        sources_by_rel[_to_src_rel_posix(p)] = p

    index = {'': {}}
    with (
        action_helpers.atomic_output(options.output) as out_file,
        zipfile.ZipFile(out_file, 'w') as zf,
    ):
        for rel_py in sorted(sources_by_rel):
            fs_path = sources_by_rel[rel_py]
            with open(fs_path, 'rb') as src_file:
                source_bytes = src_file.read()
            display_path = os.path.relpath(fs_path).replace(os.sep, '/')
            zip_helpers.add_to_zip_hermetic(
                zf,
                '_pyz/' + rel_py + 'c',
                data=_compile_pyc(source_bytes, display_path),
            )

            # Seed ancestor directories with `_KIND_NAMESPACE` via
            # `setdefault` so directories without `__init__.py` work as
            # PEP 420 namespace packages without clobbering `_KIND_PACKAGE`
            # if `pkg/__init__.py` was already processed before `pkg/z.py`.
            dir_rel = posixpath.dirname(rel_py)
            index.setdefault(dir_rel, {})
            cur = dir_rel
            while cur:
                parent = posixpath.dirname(cur)
                index.setdefault(parent, {}).setdefault(
                    posixpath.basename(cur), _KIND_NAMESPACE
                )
                cur = parent

            base = posixpath.basename(rel_py)
            if base != '__init__.py':
                index[dir_rel][base[:-3]] = _KIND_MODULE
            elif dir_rel:
                parent_rel = posixpath.dirname(dir_rel)
                index[parent_rel][posixpath.basename(dir_rel)] = _KIND_PACKAGE

        output_dir_abs = os.path.dirname(os.path.abspath(options.output))
        src_root_rel = os.path.relpath(_SRC_ROOT, output_dir_abs).replace(
            os.sep, '/'
        )
        mp_main_src = _MP_MAIN_TEMPLATE.format(
            src_root_rel=src_root_rel,
            index=index,
            entry_rel=entry_rel,
            kind_module=_KIND_MODULE,
            kind_namespace=_KIND_NAMESPACE,
        ).encode('utf-8')
        zip_helpers.add_to_zip_hermetic(
            zf,
            '_pyz_mp_main.pyc',
            data=_compile_pyc(mp_main_src, '_pyz_mp_main.py'),
        )
        zip_helpers.add_to_zip_hermetic(
            zf,
            '__main__.pyc',
            data=_compile_pyc(_MAIN_SRC, '__main__.py'),
        )

    if not options.depfile:
        return

    # Record the build's Python interpreter in the depfile when it lives under
    # `//` (e.g. `//third_party/cpython3`) so a Python version roll invalidates
    # `.pyz` files whose embedded `MAGIC_NUMBER` changed.
    depfile_deps = []
    if sys.executable:
        exe_abs = os.path.abspath(sys.executable)
        if os.path.normcase(exe_abs).startswith(
            os.path.normcase(os.path.join(_SRC_ROOT, ''))
        ):
            depfile_deps.append(os.path.relpath(exe_abs))
    action_helpers.write_depfile(options.depfile, options.output, depfile_deps)


if __name__ == '__main__':
    sys.exit(main())

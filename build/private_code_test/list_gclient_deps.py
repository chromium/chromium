#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import posixpath
import traceback

# Entries in //DEPS `recursedeps` that have been audited.
# LINT.IfChange(recursedeps)
_KNOWN_RECURSEDEPS = (
    # Internal repos: already listed as chrome-internal entries in //DEPS (so
    # any sub-deps under their directory via `use_relative_paths = True` are
    # already covered by prefix matching), and checked-out internal DEPS files
    # on supported platforms are also passed via `--deps-file` from BUILD.gn.
    'src/clank',
    'src/components/optimization_guide/internal',
    'src/ios_internal',
    # Public repos: do not contain chrome-internal dependencies used in
    # Chromium builds.
    'src/third_party/angle',
    'src/third_party/dawn',
    'src/third_party/devtools-frontend/src',
    'src/third_party/instrumented_libs',
    'src/third_party/openscreen/src',
    'src/third_party/webpagereplay',
)
# LINT.ThenChange(/DEPS:recursedeps)


def _parse_deps_file(deps_path, root_dir, source_filter):
    text = deps_path.read_text(encoding='utf-8')
    scope = {'Str': str, 'Var': lambda name: scope['vars'][name]}
    exec(text, scope)

    # By default, paths in DEPS files are relative to the .gclient root (e.g.
    # 'src/foo'). When `use_relative_paths = True` is set in a recursed DEPS
    # file (e.g. //clank/DEPS), paths in `deps` and `recursedeps` are relative
    # to that DEPS file's directory and must be prefixed with `rel_prefix`
    # ('src/clank') to normalize them to .gclient-root-relative paths.
    rel_dir = deps_path.parent.relative_to(root_dir).as_posix()
    rel_prefix = posixpath.normpath(posixpath.join('src', rel_dir))
    use_relative_paths = scope.get('use_relative_paths', False)

    def resolve_path(p):
        if use_relative_paths and isinstance(p, str):
            return posixpath.normpath(posixpath.join(rel_prefix, p))
        return p

    unknown = [
        resolved
        for e in scope.get('recursedeps', [])
        if (resolved := resolve_path(e)) not in _KNOWN_RECURSEDEPS
    ]
    if unknown:
        raise ValueError(
            f'Unhandled recursedeps in {deps_path}: {unknown}. '
            'Update //build/private_code_test/BUILD.gn and '
            'list_gclient_deps.py.'
        )

    vars_dict = scope.get('vars', {})
    matched_dirs = []
    for path, dep in scope.get('deps', {}).items():
        raw_url = (dep.get('url') if isinstance(dep, dict) else dep) or ''
        if source_filter in raw_url.format_map(vars_dict):
            matched_dirs.append(resolve_path(path.format_map(vars_dict)))
    return matched_dirs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--deps-file', action='append', required=True, dest='deps_files'
    )
    parser.add_argument('--source-filter', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    # Ninja validates that the files exist since they are marked as inputs.
    try:
        deps_paths = [pathlib.Path(p) for p in args.deps_files]
        root_dir = deps_paths[0].parent
        private_dirs = set()
        for deps_path in deps_paths:
            private_dirs.update(
                _parse_deps_file(deps_path, root_dir, args.source_filter)
            )
        private_dirs = sorted(private_dirs)
    except Exception:
        # Make the test fail rather than the compile step so that failures here
        # do not prevent other bot functionality.
        private_dirs = [
            '# ERROR: Failed parsing DEPS',
            traceback.format_exc(),
        ]

    pathlib.Path(args.output).write_text(
        '\n'.join(private_dirs) + '\n', encoding='utf-8'
    )


if __name__ == '__main__':
    main()

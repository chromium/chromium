# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Determines which files may contain the definitions of listed features.

Paths handled here are relative to the current working directory (the build
output directory), which is what ninja depfiles require.
"""

import dataclasses
import os
from typing import List, Tuple

import models


@dataclasses.dataclass(frozen=True)
class PathContext:
    source_root: str
    root_gen_dir: str

    def display(self, path: str) -> str:
        """Formats the given path relative to source_root or root_gen_dir."""
        rel_gen = os.path.relpath(path, self.root_gen_dir)
        if not rel_gen.startswith('..'):
            return '$root_gen_dir/' + rel_gen.replace(os.sep, '/')
        rel_src = os.path.relpath(path, self.source_root)
        if not rel_src.startswith('..'):
            return '//' + rel_src.replace(os.sep, '/')
        return path


def automatic_candidates(source: models.ParsedSource,
                         ctx: PathContext) -> List[str]:
    """The source itself plus the .cc of each quoted .h include that exists."""
    candidates = [os.path.normpath(source.path)]
    for include in source.includes:
        stem, ext = os.path.splitext(include)
        if ext != '.h':
            continue
        for base in (ctx.source_root, os.path.dirname(source.path)):
            cc = os.path.normpath(os.path.join(base, stem + '.cc'))
            if os.path.isfile(cc):
                if cc not in candidates:
                    candidates.append(cc)
                break
    return candidates


def resolve_directives(
        source: models.ParsedSource, candidates: List[str],
        ctx: PathContext) -> List[Tuple[models.DefinitionDirective, str]]:
    """Maps each FEATURE_DEFINITION_FILE directive to an existing file.
    The source tree is checked first, then root_gen_dir (for generated files).
    """
    resolved = []
    seen = {}
    errors = []
    for directive in source.directives:
        where = f'{directive.source_path}:{directive.line}'
        rel = directive.written_path[2:]
        in_source = os.path.normpath(os.path.join(ctx.source_root, rel))
        in_gen = os.path.normpath(os.path.join(ctx.root_gen_dir, rel))
        if os.path.isfile(in_source):
            path = in_source
        elif os.path.isfile(in_gen):
            path = in_gen
        else:
            errors.append(
                f'{where}: error: FEATURE_DEFINITION_FILE '
                f'{directive.written_path} does not exist. Checked:\n'
                f'    {in_source}\n'
                f'    {in_gen}\n'
                f'If the file is generated, add the GN target that generates '
                f'it to the `deps` of the java_flag_generator() target.')
            continue
        if path in seen:
            errors.append(f'{where}: error: Duplicate FEATURE_DEFINITION_FILE '
                          f'{directive.written_path} (first on line '
                          f'{seen[path]}).')
            continue
        if path in candidates:
            errors.append(
                f'{where}: error: FEATURE_DEFINITION_FILE '
                f'{directive.written_path} is redundant: it is already '
                f'searched automatically. Remove this comment.')
            continue
        seen[path] = directive.line
        resolved.append((directive, path))
    if errors:
        raise models.GeneratorError(errors)
    return resolved

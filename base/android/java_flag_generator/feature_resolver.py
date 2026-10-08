# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Matches kFeaturesExposedToJava entries to their feature definitions."""

import collections
from typing import Dict, List, Tuple

from definition_files import PathContext
import models

_HELP_TEXT = """\
To fix, add a comment of the form
    // FEATURE_DEFINITION_FILE: //path/to/file.cc
to {source}, where file.cc contains the feature's BASE_FEATURE() definition.
If file.cc is a generated file, also add the GN target that generates it to
the `deps` of the java_flag_generator() target."""


def _lookup_names(entry: models.FeatureEntry,
                  array_namespace: Tuple[str, ...]) -> List[str]:
    """Qualified names to try, innermost scope first, like C++ name lookup."""
    if entry.qualifier.startswith('::'):
        return [entry.qualifier[2:] + entry.identifier]
    written = [c for c in entry.qualifier.split('::') if c]
    return [
        '::'.join(list(array_namespace[:i]) + written + [entry.identifier])
        for i in range(len(array_namespace), -1, -1)
    ]


def _describe(definitions: List[models.FeatureDefinition]) -> str:
    return '\n'.join(f'    {d.path}:{d.line}: {d.qualified_name} -> '
                     f'{d.name if d.name is not None else "(unknown)"}'
                     for d in definitions)


def resolve(
    source: models.ParsedSource,
    definitions_by_file: Dict[str, List[models.FeatureDefinition]],
    candidates: List[str],
    directive_files: List[Tuple[models.DefinitionDirective,
                                str]], ctx: PathContext
) -> List[Tuple[models.FeatureEntry, models.FeatureDefinition]]:
    """Returns (entry, definition) pairs in entry order.
    Raises GeneratorError listing every problem, including all missing features.
    """
    searched = candidates + [path for _, path in directive_files]
    by_qualified = collections.defaultdict(list)
    by_identifier = collections.defaultdict(list)
    for path in searched:
        for definition in definitions_by_file[path]:
            by_qualified[definition.qualified_name].append(definition)
            by_identifier[definition.identifier].append(definition)

    results = []
    missing = []
    used_files = set()
    errors = []
    for entry in source.entries:
        where = f'{entry.source_path}:{entry.line}'
        matches = []
        for name in _lookup_names(entry, source.array_namespace):
            if name in by_qualified:
                matches = by_qualified[name]
                break
        else:
            # Fall back to the bare identifier, but never guess between
            # different features.
            matches = by_identifier.get(entry.identifier, [])
            if len({d.qualified_name for d in matches}) > 1:
                errors.append(f'{where}: error: Cannot tell which definition '
                              f'"{entry.written}" refers to. Candidates:\n'
                              f'{_describe(matches)}')
                continue

        if not matches:
            missing.append(f'{where}: error: Cannot find the definition of '
                           f'"{entry.written}".')
            continue
        if len({(d.name, d.error) for d in matches}) > 1:
            errors.append(f'{where}: error: "{entry.written}" has conflicting '
                          f'definitions:\n{_describe(matches)}')
            continue
        definition = matches[0]
        if definition.error:
            errors.append(f'{where}: error: {definition.error}')
            continue
        used_files.update(d.path for d in matches)
        results.append((entry, definition))

    for directive, path in directive_files:
        if path not in used_files:
            errors.append(
                f'{directive.source_path}:{directive.line}: error: '
                f'FEATURE_DEFINITION_FILE {directive.written_path} is unused: '
                f'no entry in {source.path} is defined there. Please remove '
                f'this comment.')

    if missing:
        searched_list = '\n'.join(f'    {ctx.display(p)}' for p in searched)
        errors.extend(missing)
        errors.append(f'Searched these files:\n{searched_list}\n' +
                      _HELP_TEXT.format(source=ctx.display(source.path)))
    if errors:
        raise models.GeneratorError(errors)
    return results

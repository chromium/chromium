# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Matches k*ExposedToJava entries to their feature and param definitions."""

import collections
from typing import Dict, List, NamedTuple, Optional, Set, Tuple, Union

from definition_files import PathContext
import models

_HELP_TEXT = """\
To fix, add a comment of the form
    // FEATURE_DEFINITION_FILE: //path/to/file.cc
to {source},
where file.cc contains the feature's BASE_FEATURE() or the param's
BASE_FEATURE_PARAM() definition.
If file.cc is a generated file, also add the GN target that generates it to
the `deps` of the java_flag_generator() target."""

_Definition = Union[models.FeatureDefinition, models.ParamDefinition]


class FeatureMatch(NamedTuple):
    """A kFeaturesExposedToJava entry and the definition it refers to."""

    entry: models.ArrayEntry
    definition: models.FeatureDefinition


class ParamMatch(NamedTuple):
    """A k<Type>ParamsExposedToJava entry and the definition it refers to."""

    param_type: models.ParamType
    entry: models.ArrayEntry
    definition: models.ParamDefinition
    # The definition of the param's feature.
    feature: models.FeatureDefinition


class _Index:
    """The feature or param definitions in the searched files."""

    def __init__(self, definitions: List[_Definition]):
        self._by_qualified = collections.defaultdict(list)
        self._by_identifier = collections.defaultdict(list)
        for definition in definitions:
            self._by_qualified[definition.qualified_name].append(definition)
            self._by_identifier[definition.identifier].append(definition)

    def _lookup_names(
        self, entry: models.ArrayEntry, scope: Tuple[str, ...]
    ) -> List[str]:
        """Qualified names to try, innermost scope first, like C++ name
        lookup.
        """
        if entry.qualifier.startswith('::'):
            return [entry.qualifier[2:] + entry.identifier]
        written = [c for c in entry.qualifier.split('::') if c]
        return [
            '::'.join(list(scope[:i]) + written + [entry.identifier])
            for i in range(len(scope), -1, -1)
        ]

    def lookup(
        self, entry: models.ArrayEntry, scope: Tuple[str, ...]
    ) -> List[_Definition]:
        for name in self._lookup_names(entry, scope):
            if name in self._by_qualified:
                return self._by_qualified[name]
        # Fall back to the bare identifier. The caller never guesses between
        # different definitions.
        return self._by_identifier.get(entry.identifier, [])

    def describe(self, definitions: List[_Definition]) -> str:
        lines = []
        for d in definitions:
            lines.append(
                f'    {d.path}:{d.line}: {d.qualified_name} -> '
                f'{d.name if d.name is not None else "(unknown)"}'
            )
        return '\n'.join(lines)


class _Matcher:
    """Matches entries to definitions, and collects the problems."""

    def __init__(self):
        self.errors: List[str] = []
        self.missing: List[str] = []
        self.used_files: Set[str] = set()

    def match(
        self,
        entry: models.ArrayEntry,
        scope: Tuple[str, ...],
        index: _Index,
        where: str,
        subject: str,
    ) -> Optional[_Definition]:
        """Returns the definition that `entry` refers to when looked up from
        `scope`. Otherwise, records the problem and returns None. Problems are
        reported at `where` and refer to the entry as `subject`.
        """
        matches = index.lookup(entry, scope)
        if len({d.qualified_name for d in matches}) > 1:
            self.errors.append(
                f'{where}: error: Cannot tell which definition '
                f'{subject} refers to. Candidates:\n'
                f'{index.describe(matches)}'
            )
            return None
        if not matches:
            self.missing.append(
                f'{where}: error: Cannot find the definition of {subject}.'
            )
            return None
        if len({(d.name, d.error) for d in matches}) > 1:
            self.errors.append(
                f'{where}: error: {subject} has conflicting '
                f'definitions:\n{index.describe(matches)}'
            )
            return None
        definition = matches[0]
        if definition.error:
            self.errors.append(f'{where}: error: {definition.error}')
            return None
        self.used_files.update(d.path for d in matches)
        return definition


def resolve(
    source: models.ParsedSource,
    feature_definitions_by_file: Dict[str, List[models.FeatureDefinition]],
    param_definitions_by_file: Dict[str, List[models.ParamDefinition]],
    candidates: List[str],
    directive_files: List[Tuple[models.DefinitionDirective, str]],
    ctx: PathContext,
) -> Tuple[List[FeatureMatch], List[ParamMatch]]:
    """Returns the definitions of the entries of kFeaturesExposedToJava as
    FeatureMatches, and those of the k<Type>ParamsExposedToJava arrays as
    ParamMatches, both in entry order.
    Raises GeneratorError listing every problem, including all missing
    definitions.
    """
    searched = candidates + [path for _, path in directive_files]
    features_index = _Index(
        [d for path in searched for d in feature_definitions_by_file[path]]
    )
    params_index = _Index(
        [d for path in searched for d in param_definitions_by_file[path]]
    )
    matcher = _Matcher()

    feature_matches = []
    for entry in source.feature_array.entries:
        where = f'{entry.source_path}:{entry.line}'
        definition = matcher.match(
            entry,
            source.feature_array.namespace,
            features_index,
            where,
            f'"{entry.written}"',
        )
        if definition is not None:
            feature_matches.append(FeatureMatch(entry, definition))

    param_matches = []
    for array in source.param_arrays:
        for entry in array.entries:
            where = f'{entry.source_path}:{entry.line}'
            definition = matcher.match(
                entry,
                array.namespace,
                params_index,
                where,
                f'"{entry.written}"',
            )
            if definition is None:
                continue
            # The feature is looked up from the param's definition.
            feature = matcher.match(
                definition.feature,
                definition.namespace,
                features_index,
                where,
                f'"{definition.feature.written}" (the feature of '
                f'{definition.identifier} at {definition.path}:'
                f'{definition.line})',
            )
            if feature is not None:
                param_matches.append(
                    ParamMatch(array.param_type, entry, definition, feature)
                )

    errors = matcher.errors
    for directive, path in directive_files:
        if path not in matcher.used_files:
            errors.append(
                f'{directive.source_path}:{directive.line}: error: '
                f'FEATURE_DEFINITION_FILE {directive.written_path} is unused: '
                f'no entry in {source.path} is defined there. Please remove '
                f'this comment.'
            )

    if matcher.missing:
        searched_list = '\n'.join(f'    {ctx.display(p)}' for p in searched)
        errors.extend(matcher.missing)
        errors.append(
            f'Searched these files:\n{searched_list}\n'
            + _HELP_TEXT.format(source=ctx.display(source.path))
        )
    if errors:
        raise models.GeneratorError(errors)
    return feature_matches, param_matches

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses C++ feature list files and feature definition files."""

import bisect
import re
from typing import List, Tuple

import cpp_lexer
import models

_ARRAY_NAME = 'kFeaturesExposedToJava'
_ARRAY_RE = re.compile(r'\b' + _ARRAY_NAME + r'\s*(?:\[[^\]]*\])?\s*=\s*\{')
_ENTRY_RE = re.compile(r'^&((?:::)?(?:\w+::)*)(k[A-Z]\w*)$')
_DIRECTIVE_RE = re.compile(r'^[ \t]*//[ \t]*FEATURE_DEFINITION_FILE:(.*)$',
                           re.MULTILINE)
_DIRECTIVE_PATH_RE = re.compile(r'^//[^\s]+$')
_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.MULTILINE)
_NAMESPACE_OR_BRACE_RE = re.compile(
    r'\b(?P<inline>inline\s+)?namespace\b\s*(?P<name>[\w:]*)\s*\{|[{}]')
_JNI_GET_NATIVE_MAP_RE = re.compile(r'\bJNI_(\w+)_GetNativeMap\s*\(')
_MACRO_CALL_RE = re.compile(r'\b([A-Z][A-Z0-9_]*)\s*\(')
_K_IDENTIFIER_RE = re.compile(r'^k[A-Z]\w*$')
_STRING_LITERAL_RE = re.compile(r'^"((?:[^"\\\n]|\\.)*)"$')


class _NamespaceTracker:
    """Answers "which namespace encloses this offset?" for a structure view."""

    def __init__(self, structure: str):
        self._positions = [0]
        self._namespaces = [()]
        # Each element is a tuple of namespace components pushed by that brace.
        stack = []
        for m in _NAMESPACE_OR_BRACE_RE.finditer(structure):
            token = m.group()
            if token == '}':
                if stack:
                    stack.pop()
            elif token == '{':
                stack.append(())
            else:
                name = m.group('name')
                # Anonymous and inline namespaces don't add a lookup component.
                if m.group('inline') or not name:
                    stack.append(())
                else:
                    stack.append(tuple(c for c in name.split('::') if c))
            self._positions.append(m.end())
            self._namespaces.append(tuple(c for frame in stack for c in frame))

    def at(self, index: int) -> Tuple[str, ...]:
        return self._namespaces[bisect.bisect_right(self._positions, index) - 1]


class _LineNumberTracker:
    """Maps indices in a file (or any of its lex() views) to line numbers."""

    def __init__(self, text: str):
        self._line_starts = [0] + [m.end() for m in re.finditer('\n', text)]

    def at(self, index: int) -> int:
        # The number of line starts at or before `index` is its line number.
        return bisect.bisect_right(self._line_starts, index)


def _iter_lines_with_offsets(text: str):
    offset = 0
    for line in text.split('\n'):
        yield offset, line
        offset += len(line) + 1


def _parse_entries(path: str, structure: str, line_numbers: _LineNumberTracker,
                   body: Tuple[int, int]) -> List[models.FeatureEntry]:
    body_start, body_end = body
    body_text = structure[body_start:body_end]
    errors = []
    for offset, line_text in _iter_lines_with_offsets(body_text):
        if line_text.lstrip().startswith('#'):
            line = line_numbers.at(body_start + offset)
            errors.append(
                f'{path}:{line}: error: Preprocessor directives are not '
                f'supported inside {_ARRAY_NAME}, because the generated '
                f'Java class is not conditional.')
    if errors:
        raise models.GeneratorError(errors)

    entries = []
    seen = {}
    pieces = cpp_lexer.split_top_level(structure, body_start, body_end)
    for idx, (start, end) in enumerate(pieces):
        raw = structure[start:end]
        normalized = re.sub(r'\s+', '', raw)
        if not normalized:
            # Only a trailing comma may produce an empty piece.
            if idx != len(pieces) - 1:
                line = line_numbers.at(end)
                errors.append(
                    f'{path}:{line}: error: Empty entry in {_ARRAY_NAME}.')
            continue
        line = line_numbers.at(start + len(raw) - len(raw.lstrip()))
        m = _ENTRY_RE.match(normalized)
        if not m:
            errors.append(
                f'{path}:{line}: error: Unsupported entry "{normalized}" in '
                f'{_ARRAY_NAME}. Expected the form "&namespace::kFeatureName".')
            continue
        entry = models.FeatureEntry(path, line, m.group(1), m.group(2))
        if entry.written in seen:
            errors.append(
                f'{path}:{line}: error: Duplicate entry "{entry.written}" '
                f'(first listed on line {seen[entry.written]}).')
            continue
        seen[entry.written] = line
        entries.append(entry)
    if errors:
        raise models.GeneratorError(errors)
    return entries


def _parse_directives(
        path: str, text: str,
        line_numbers: _LineNumberTracker) -> List[models.DefinitionDirective]:
    directives = []
    errors = []
    for m in _DIRECTIVE_RE.finditer(text):
        value = m.group(1).strip()
        line = line_numbers.at(m.start())
        if not _DIRECTIVE_PATH_RE.match(value):
            errors.append(
                f'{path}:{line}: error: FEATURE_DEFINITION_FILE must be '
                f'followed by a source-absolute path such as '
                f'"//path/to/file.cc", found "{value}".')
            continue
        directives.append(models.DefinitionDirective(path, line, value))
    if errors:
        raise models.GeneratorError(errors)
    return directives


def parse_source(path: str, text: str) -> models.ParsedSource:
    """Parses a C++ file that defines kFeaturesExposedToJava."""
    code, structure = cpp_lexer.lex(text)
    line_numbers = _LineNumberTracker(text)

    matches = list(_ARRAY_RE.finditer(structure))
    if not matches:
        raise models.GeneratorError([
            f'{path}: error: Cannot find the definition of {_ARRAY_NAME}.'
        ])
    if len(matches) > 1:
        lines = ', '.join(str(line_numbers.at(m.start())) for m in matches)
        raise models.GeneratorError([
            f'{path}: error: Found multiple definitions of {_ARRAY_NAME} '
            f'(lines {lines}).'
        ])
    array_match = matches[0]
    open_brace = array_match.end() - 1
    try:
        close_brace = cpp_lexer.find_matching(structure, open_brace)
    except ValueError as e:
        line = line_numbers.at(open_brace)
        raise models.GeneratorError(
            [f'{path}:{line}: error: Cannot parse {_ARRAY_NAME}: {e}.']) from e

    entries = _parse_entries(path, structure, line_numbers,
                             (open_brace + 1, close_brace))
    directives = _parse_directives(path, text, line_numbers)
    includes = tuple(m.group(1) for m in _INCLUDE_RE.finditer(code))

    _, blanked_structure = cpp_lexer.blank_preprocessor_lines(code, structure)
    array_namespace = _NamespaceTracker(blanked_structure).at(
        array_match.start())

    feature_map_names = tuple(
        sorted({
            m.group(1)
            for m in _JNI_GET_NATIVE_MAP_RE.finditer(blanked_structure)
        }))

    return models.ParsedSource(
        path=path,
        entries=tuple(entries),
        array_namespace=array_namespace,
        includes=includes,
        directives=tuple(directives),
        feature_map_names=feature_map_names)


def _definition_from_macro(path: str, line: int, namespace: Tuple[str, ...],
                           macro: str, identifier: str,
                           args: List[str]) -> models.FeatureDefinition:
    derived = identifier[1:]
    where = f'{path}:{line}'
    if macro == 'BASE_FEATURE' or macro == 'BASE_RUNTIME_MUTABLE_FEATURE':
        if len(args) == 2:
            return models.FeatureDefinition(path, line, namespace, identifier,
                                            macro, derived)
        if len(args) == 3:
            m = _STRING_LITERAL_RE.match(args[1])
            if m:
                return models.FeatureDefinition(path, line, namespace,
                                                identifier, macro, m.group(1))
            return models.FeatureDefinition(
                path, line, namespace, identifier, macro, None,
                f'{where}: The feature name of {identifier} must be a plain '
                f'string literal, found "{args[1]}".')
        return models.FeatureDefinition(
            path, line, namespace, identifier, macro, None,
            f'{where}: {macro}({identifier}, ...) has {len(args)} '
            f'arguments; expected 2 or 3.')

    if len(args) >= 2 and _STRING_LITERAL_RE.match(args[1]):
        return models.FeatureDefinition(
            path, line, namespace, identifier, macro, None,
            f'{where}: Cannot determine the feature name of {identifier} from '
            f'macro {macro}, because it is passed a string literal.')
    return models.FeatureDefinition(path, line, namespace, identifier, macro,
                                    derived)


def parse_definitions(path: str, text: str) -> List[models.FeatureDefinition]:
    """Finds feature definitions such as BASE_FEATURE(kFoo, ...) in a file.
    A definition is any ALL_CAPS macro call whose name contains FEATURE (but not
    DECLARE or PARAM) and whose first argument is a kCamelCase identifier.
    """
    code, structure = cpp_lexer.lex(text)
    # Preprocessor lines often sit between BASE_FEATURE arguments, e.g.
    #   BASE_FEATURE(kFoo,
    #   #if BUILDFLAG(IS_ANDROID)
    #                base::FEATURE_ENABLED_BY_DEFAULT);
    #   #else
    #                base::FEATURE_DISABLED_BY_DEFAULT);
    #   #endif
    code, structure = cpp_lexer.blank_preprocessor_lines(code, structure)
    namespaces = _NamespaceTracker(structure)
    line_numbers = _LineNumberTracker(text)

    definitions = []
    for m in _MACRO_CALL_RE.finditer(structure):
        macro = m.group(1)
        if 'FEATURE' not in macro or 'DECLARE' in macro or 'PARAM' in macro:
            continue
        open_paren = m.end() - 1
        try:
            close_paren = cpp_lexer.find_matching(structure, open_paren)
        except ValueError:
            continue
        ranges = cpp_lexer.split_top_level(structure, open_paren + 1,
                                           close_paren)
        args = [code[s:e].strip() for s, e in ranges]
        if not args or not _K_IDENTIFIER_RE.match(args[0]):
            continue
        line = line_numbers.at(m.start())
        definitions.append(
            _definition_from_macro(path, line, namespaces.at(m.start()), macro,
                                   args[0], args))
    return definitions

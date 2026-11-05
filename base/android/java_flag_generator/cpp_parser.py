# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses C++ feature list files, and feature and param definition files."""

import bisect
import re
from typing import Callable, Iterator, List, NamedTuple, Optional, Tuple

import cpp_lexer
import models

_FEATURE_ARRAY_NAME = 'kFeaturesExposedToJava'
_PARAM_MACRO = 'BASE_FEATURE_PARAM'
_ENTRY_RE = re.compile(r'^&((?:::)?(?:\w+::)*)(k[A-Z]\w*)$')
_DIRECTIVE_RE = re.compile(
    r'^[ \t]*//[ \t]*FEATURE_DEFINITION_FILE:(.*)$', re.MULTILINE
)
_DIRECTIVE_PATH_RE = re.compile(r'^//[^\s]+$')
_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.MULTILINE)
_NAMESPACE_OR_BRACE_RE = re.compile(
    r'\b(?P<inline>inline\s+)?namespace\b\s*(?P<name>[\w:]*)\s*\{|[{}]'
)
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


def _parse_entries(
    path: str,
    text: str,
    structure: str,
    line_numbers: _LineNumberTracker,
    array_name: str,
    body: Tuple[int, int],
) -> List[models.ArrayEntry]:
    body_start, body_end = body
    # Preprocessor lines are blanked in `structure`, so look for them in `text`.
    body_text = text[body_start:body_end]
    errors = []
    for offset, line_text in _iter_lines_with_offsets(body_text):
        if line_text.lstrip().startswith('#'):
            line = line_numbers.at(body_start + offset)
            errors.append(
                f'{path}:{line}: error: Preprocessor directives are not '
                f'supported inside {array_name}, because the generated '
                f'Java class is not conditional.'
            )
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
                    f'{path}:{line}: error: Empty entry in {array_name}.'
                )
            continue
        line = line_numbers.at(start + len(raw) - len(raw.lstrip()))
        m = _ENTRY_RE.match(normalized)
        if not m:
            errors.append(
                f'{path}:{line}: error: Unsupported entry "{normalized}" in '
                f'{array_name}. Expected the form "&namespace::kSomeName".'
            )
            continue
        entry = models.ArrayEntry(path, line, m.group(1), m.group(2))
        if entry.written in seen:
            errors.append(
                f'{path}:{line}: error: Duplicate entry "{entry.written}" '
                f'(first listed on line {seen[entry.written]}).'
            )
            continue
        seen[entry.written] = line
        entries.append(entry)
    if errors:
        raise models.GeneratorError(errors)
    return entries


def _parse_directives(
    path: str, text: str, line_numbers: _LineNumberTracker
) -> List[models.DefinitionDirective]:
    directives = []
    errors = []
    for m in _DIRECTIVE_RE.finditer(text):
        value = m.group(1).strip()
        line = line_numbers.at(m.start())
        if not _DIRECTIVE_PATH_RE.match(value):
            errors.append(
                f'{path}:{line}: error: FEATURE_DEFINITION_FILE must be '
                f'followed by a source-absolute path such as '
                f'"//path/to/file.cc", found "{value}".'
            )
            continue
        directives.append(models.DefinitionDirective(path, line, value))
    if errors:
        raise models.GeneratorError(errors)
    return directives


def _parse_array(
    path: str,
    text: str,
    structure: str,
    line_numbers: _LineNumberTracker,
    namespaces: _NamespaceTracker,
    param_type: Optional[models.ParamType],
    required: bool,
) -> Optional[models.Array]:
    """Parses the definition of the k<Type>ParamsExposedToJava array of
    `param_type`, or of kFeaturesExposedToJava if `param_type` is None.
    Returns None if the array is not defined and not `required`.
    """
    if param_type is None:
        array_name = _FEATURE_ARRAY_NAME
    else:
        array_name = param_type.array_name
    array_re = re.compile(r'\b' + array_name + r'\s*(?:\[[^\]]*\])?\s*=\s*\{')
    matches = list(array_re.finditer(structure))
    if not matches:
        if not required:
            return None
        raise models.GeneratorError(
            [f'{path}: error: Cannot find the definition of {array_name}.']
        )
    if len(matches) > 1:
        lines = ', '.join(str(line_numbers.at(m.start())) for m in matches)
        raise models.GeneratorError(
            [
                f'{path}: error: Found multiple definitions of {array_name} '
                f'(lines {lines}).'
            ]
        )
    array_match = matches[0]
    open_brace = array_match.end() - 1
    try:
        close_brace = cpp_lexer.find_matching(structure, open_brace)
    except ValueError as e:
        line = line_numbers.at(open_brace)
        raise models.GeneratorError(
            [f'{path}:{line}: error: Cannot parse {array_name}: {e}.']
        ) from e

    entries = _parse_entries(
        path,
        text,
        structure,
        line_numbers,
        array_name,
        (open_brace + 1, close_brace),
    )
    return models.Array(
        param_type, namespaces.at(array_match.start()), tuple(entries)
    )


def parse_source(path: str, text: str, structure: str) -> models.ParsedSource:
    """Parses a C++ file that defines kFeaturesExposedToJava, and optionally
    k<Type>ParamsExposedToJava arrays. `structure` is the structure view of
    `text` (see cpp_lexer.lex()) with preprocessor lines blanked.
    """
    line_numbers = _LineNumberTracker(text)
    namespaces = _NamespaceTracker(structure)

    # Report the errors of all arrays at once.
    errors = []
    try:
        feature_array = _parse_array(
            path,
            text,
            structure,
            line_numbers,
            namespaces,
            param_type=None,
            required=True,
        )
    except models.GeneratorError as e:
        errors.extend(e.messages)

    param_arrays = []
    for param_type in models.PARAM_TYPES:
        try:
            param_array = _parse_array(
                path,
                text,
                structure,
                line_numbers,
                namespaces,
                param_type,
                required=False,
            )
        except models.GeneratorError as e:
            errors.extend(e.messages)
            param_array = None
        if param_array is not None:
            param_arrays.append(param_array)
    if errors:
        raise models.GeneratorError(errors)

    includes = tuple(m.group(1) for m in _INCLUDE_RE.finditer(text))
    directives = _parse_directives(path, text, line_numbers)

    feature_map_names = tuple(
        sorted({m.group(1) for m in _JNI_GET_NATIVE_MAP_RE.finditer(structure)})
    )

    return models.ParsedSource(
        path=path,
        includes=includes,
        directives=tuple(directives),
        feature_map_names=feature_map_names,
        feature_array=feature_array,
        param_arrays=tuple(param_arrays),
    )


def _feature_definition_from_macro(
    path: str,
    line: int,
    namespace: Tuple[str, ...],
    macro: str,
    identifier: str,
    args: List[str],
) -> models.FeatureDefinition:
    derived = identifier[1:]
    where = f'{path}:{line}'
    if macro == 'BASE_FEATURE' or macro == 'BASE_RUNTIME_MUTABLE_FEATURE':
        if len(args) == 2:
            return models.FeatureDefinition(
                path, line, namespace, identifier, macro, derived
            )
        if len(args) == 3:
            m = _STRING_LITERAL_RE.match(args[1])
            if m:
                return models.FeatureDefinition(
                    path, line, namespace, identifier, macro, m.group(1)
                )
            return models.FeatureDefinition(
                path,
                line,
                namespace,
                identifier,
                macro,
                None,
                f'{where}: The feature name of {identifier} must be a plain '
                f'string literal, found "{args[1]}".',
            )
        return models.FeatureDefinition(
            path,
            line,
            namespace,
            identifier,
            macro,
            None,
            f'{where}: {macro}({identifier}, ...) has {len(args)} '
            f'arguments; expected 2 or 3.',
        )

    if len(args) >= 2 and _STRING_LITERAL_RE.match(args[1]):
        return models.FeatureDefinition(
            path,
            line,
            namespace,
            identifier,
            macro,
            None,
            f'{where}: Cannot determine the feature name of {identifier} from '
            f'macro {macro}, because it is passed a string literal.',
        )
    return models.FeatureDefinition(
        path, line, namespace, identifier, macro, derived
    )


def _param_definition_from_macro(
    path: str, line: int, namespace: Tuple[str, ...], args: List[str]
) -> models.ParamDefinition:
    """Reads BASE_FEATURE_PARAM(T, kFoo, &kFeature, [name,] default_value)."""
    identifier = args[1]
    where = f'{path}:{line}'

    def invalid(reason: str) -> models.ParamDefinition:
        return models.ParamDefinition(
            path, line, namespace, identifier, None, None, f'{where}: {reason}'
        )

    if len(args) not in (4, 5):
        return invalid(
            f'{_PARAM_MACRO}({args[0]}, {identifier}, ...) has '
            f'{len(args)} arguments; expected 4 or 5.'
        )
    m = _ENTRY_RE.match(re.sub(r'\s+', '', args[2]))
    if not m:
        return invalid(
            f'The feature of {identifier} must have the form '
            f'"&namespace::kFeatureName", found "{args[2]}".'
        )
    feature = models.ArrayEntry(path, line, m.group(1), m.group(2))
    if len(args) == 4:
        return models.ParamDefinition(
            path, line, namespace, identifier, identifier[1:], feature
        )
    m = _STRING_LITERAL_RE.match(args[3])
    if not m:
        return invalid(
            f'The param name of {identifier} must be a plain string '
            f'literal, found "{args[3]}".'
        )
    return models.ParamDefinition(
        path, line, namespace, identifier, m.group(1), feature
    )


class _MacroCall(NamedTuple):
    macro: str
    line: int
    namespace: Tuple[str, ...]
    # Arguments as written, without comments.
    args: List[str]


def _macro_calls(
    code: str, structure: str, accept: Callable[[str], bool]
) -> Iterator[_MacroCall]:
    """Yields the calls of the ALL_CAPS macros whose name passes `accept`."""
    line_numbers = _LineNumberTracker(structure)
    namespaces = _NamespaceTracker(structure)

    for m in _MACRO_CALL_RE.finditer(structure):
        macro = m.group(1)
        if not accept(macro):
            continue
        open_paren = m.end() - 1
        try:
            close_paren = cpp_lexer.find_matching(structure, open_paren)
        except ValueError:
            continue
        ranges = cpp_lexer.split_top_level(
            structure, open_paren + 1, close_paren
        )
        yield _MacroCall(
            macro,
            line_numbers.at(m.start()),
            namespaces.at(m.start()),
            [code[s:e].strip() for s, e in ranges],
        )


def parse_feature_definitions(
    path: str, code: str, structure: str
) -> List[models.FeatureDefinition]:
    """Finds feature definitions such as BASE_FEATURE(kFoo, ...) in a file.
    A definition is any ALL_CAPS macro call whose name contains FEATURE (but not
    DECLARE or PARAM) and whose first argument is a kCamelCase identifier.
    `code` and `structure` are the views of the file (see cpp_lexer.lex()) with
    preprocessor lines blanked.
    """

    def is_feature_definition_macro(macro: str) -> bool:
        return (
            'FEATURE' in macro
            and 'DECLARE' not in macro
            and 'PARAM' not in macro
        )

    definitions = []
    for call in _macro_calls(code, structure, is_feature_definition_macro):
        if not call.args or not _K_IDENTIFIER_RE.match(call.args[0]):
            continue
        definitions.append(
            _feature_definition_from_macro(
                path,
                call.line,
                call.namespace,
                call.macro,
                call.args[0],
                call.args,
            )
        )
    return definitions


def parse_param_definitions(
    path: str, code: str, structure: str
) -> List[models.ParamDefinition]:
    """Finds param definitions such as BASE_FEATURE_PARAM(int, kFoo, ...) in a
    file. A definition is any BASE_FEATURE_PARAM call whose second argument is
    a kCamelCase identifier. `code` and `structure` are the views of the file
    (see cpp_lexer.lex()) with preprocessor lines blanked.
    """
    # Avoids scanning the many files that don't define any param.
    if _PARAM_MACRO not in structure:
        return []
    definitions = []
    for call in _macro_calls(
        code, structure, lambda macro: macro == _PARAM_MACRO
    ):
        if len(call.args) < 2 or not _K_IDENTIFIER_RE.match(call.args[1]):
            continue
        definitions.append(
            _param_definition_from_macro(
                path, call.line, call.namespace, call.args
            )
        )
    return definitions

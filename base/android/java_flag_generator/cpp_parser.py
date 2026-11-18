# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses C++ feature list files, and feature and param definition files."""

import bisect
import re
from typing import (
    Callable,
    Iterator,
    List,
    NamedTuple,
    Optional,
    Tuple,
    TypeVar,
)

import cpp_lexer
import models

_FEATURE_ARRAY_NAME = 'kFeaturesExposedToJava'
_PARAM_MACRO = 'BASE_FEATURE_PARAM'
_COUNTRY_RESTRICTED_FEATURE_MACRO = 'BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS'
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

# Whether a feature is enabled by default, for each base::FeatureState. A
# country-restricted feature is enabled for some countries only, so it gets its
# state for an unknown country.
_FEATURE_STATES = {
    'FEATURE_DISABLED_BY_DEFAULT': False,
    'FEATURE_ENABLED_BY_DEFAULT': True,
    'FEATURE_DISABLED_FOR_COUNTRIES': True,
    'FEATURE_ENABLED_FOR_COUNTRIES': False,
}
# Whether the features defined by these macros, which don't take a default state
# argument, are enabled by default.
_FEATURE_MACRO_DEFAULT_VALUES = {
    'DEFINE_FEATURE_WITH_MOBILE_COUNTRY_RESTRICTION': True,
}
# A conditional expression `condition ? a : b`. Its ":" is the first one after
# the "?" that is not part of a "::".
_CONDITIONAL_RE = re.compile(r'^([^?]*)\?(.*?)(?<!:):(?!:)(.*)$', re.DOTALL)

_T = TypeVar('_T')


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
    structure: str,
    line_numbers: _LineNumberTracker,
    array_name: str,
    body: Tuple[int, int],
) -> List[models.ArrayEntry]:
    body_start, body_end = body
    # The directives of #if blocks are blanked in `structure`, but other
    # directives are not.
    errors = []
    for offset, body_line in _iter_lines_with_offsets(
        structure[body_start:body_end]
    ):
        if body_line.lstrip().startswith('#'):
            line = line_numbers.at(body_start + offset)
            errors.append(
                f'{path}:{line}: error: Unsupported preprocessor directive '
                f'inside {array_name}. Only #if, #ifdef, #ifndef, #elif, '
                f'#else and #endif are supported.'
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
        structure,
        line_numbers,
        array_name,
        (open_brace + 1, close_brace),
    )
    return models.Array(
        param_type, namespaces.at(array_match.start()), tuple(entries)
    )


def parse_source(
    path: str, text: str, code: str, structure: str
) -> models.ParsedSource:
    """Parses a C++ file that defines kFeaturesExposedToJava, and optionally
    k<Type>ParamsExposedToJava arrays. `text` is the content of the file, and
    `code` and `structure` are its views (see cpp_lexer.lex()), all without the
    code that is not compiled (see cpp_lexer.blank_inactive_branches()).
    """
    line_numbers = _LineNumberTracker(text)
    namespaces = _NamespaceTracker(structure)

    # Report the errors of all arrays at once.
    errors = []
    try:
        feature_array = _parse_array(
            path,
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

    includes = tuple(m.group(1) for m in _INCLUDE_RE.finditer(code))
    # Comments are blanked in `code` and `structure`.
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


def _feature_state(text: str) -> Optional[bool]:
    """Returns whether a feature with the default state `text`, a
    base::FeatureState value, is enabled by default, or None if unknown.
    """
    name = (
        re.sub(r'\s+', '', text)
        .removeprefix('::')
        .removeprefix('base::')
        .removeprefix('FeatureState::')
    )
    return _FEATURE_STATES.get(name)


def _param_value(cpp_type: str, text: str) -> Optional[models.ParamValue]:
    """Converts `text`, the default value of a param of type `cpp_type`, or
    returns None if it can't be converted.
    """
    try:
        if cpp_type == 'bool':
            return {'true': True, 'false': False}.get(text)
        if cpp_type == 'int':
            return int(text)
        if cpp_type == 'double':
            return float(text)
    except ValueError:
        return None
    if cpp_type == 'std::string':
        # Like names, the value is kept as written, e.g. escape sequences are
        # not decoded.
        m = _STRING_LITERAL_RE.match(text)
        return m.group(1) if m else None
    return None


def _default_value(
    text: str,
    convert: Callable[[str], Optional[_T]],
    is_desktop_android: bool,
) -> Optional[_T]:
    """Returns the default value written as `text`, converted with `convert`,
    or None if it can't be determined. `text` can also be a conditional
    expression `condition ? a : b` whose condition is made of BUILDFLAG()s.
    """
    value = convert(text)
    if value is not None:
        return value
    m = _CONDITIONAL_RE.match(text)
    if not m:
        return None
    condition = cpp_lexer.evaluate_condition(m.group(1), is_desktop_android)
    if condition is None:
        return None
    return convert((m.group(2) if condition else m.group(3)).strip())


def _feature_definition_from_macro(
    path: str,
    line: int,
    namespace: Tuple[str, ...],
    macro: str,
    identifier: str,
    args: List[str],
    is_desktop_android: bool,
) -> models.FeatureDefinition:
    where = f'{path}:{line}'

    def invalid(reason: str) -> models.FeatureDefinition:
        return models.FeatureDefinition(
            path, line, namespace, identifier, macro, None, f'{where}: {reason}'
        )

    def valid(name: str, state: Optional[str]) -> models.FeatureDefinition:
        """`state` is the argument with the default state, or None if the
        macro is not known to have one.
        """
        if state is None:
            default_value = _FEATURE_MACRO_DEFAULT_VALUES.get(macro)
        else:
            default_value = _default_value(
                state, _feature_state, is_desktop_android
            )
        return models.FeatureDefinition(
            path,
            line,
            namespace,
            identifier,
            macro,
            name,
            default_value=default_value,
        )

    derived = identifier[1:]
    if macro == 'BASE_FEATURE' or macro == 'BASE_RUNTIME_MUTABLE_FEATURE':
        if len(args) == 2:
            return valid(derived, args[1])
        if len(args) == 3:
            m = _STRING_LITERAL_RE.match(args[1])
            if m:
                return valid(m.group(1), args[2])
            return invalid(
                f'The feature name of {identifier} must be a plain string '
                f'literal, found "{args[1]}".'
            )
        return invalid(
            f'{macro}({identifier}, ...) has {len(args)} arguments; '
            f'expected 2 or 3.'
        )
    if macro == _COUNTRY_RESTRICTED_FEATURE_MACRO and len(args) >= 3:
        # BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFoo, state, countries...).
        return valid(derived, args[1])

    if len(args) >= 2 and _STRING_LITERAL_RE.match(args[1]):
        return invalid(
            f'Cannot determine the feature name of {identifier} from macro '
            f'{macro}, because it is passed a string literal.'
        )
    return valid(derived, None)


def _param_definition_from_macro(
    path: str,
    line: int,
    namespace: Tuple[str, ...],
    args: List[str],
    is_desktop_android: bool,
) -> models.ParamDefinition:
    """Reads BASE_FEATURE_PARAM(T, kFoo, &kFeature, [name,] default_value)."""
    identifier = args[1]
    where = f'{path}:{line}'

    def invalid(reason: str) -> models.ParamDefinition:
        return models.ParamDefinition(
            path, line, namespace, identifier, None, None, f'{where}: {reason}'
        )

    def valid(name: str, feature: models.ArrayEntry) -> models.ParamDefinition:
        default_value = _default_value(
            args[-1],
            lambda text: _param_value(args[0], text),
            is_desktop_android,
        )
        return models.ParamDefinition(
            path,
            line,
            namespace,
            identifier,
            name,
            feature,
            default_value=default_value,
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
        return valid(identifier[1:], feature)
    m = _STRING_LITERAL_RE.match(args[3])
    if not m:
        return invalid(
            f'The param name of {identifier} must be a plain string '
            f'literal, found "{args[3]}".'
        )
    return valid(m.group(1), feature)


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
    path: str, code: str, structure: str, is_desktop_android: bool
) -> List[models.FeatureDefinition]:
    """Finds feature definitions such as BASE_FEATURE(kFoo, ...) in a file.
    A definition is any ALL_CAPS macro call whose name contains FEATURE (but not
    DECLARE or PARAM) and whose first argument is a kCamelCase identifier.
    `code` and `structure` are the views of the file (see cpp_lexer.lex())
    without the code that is not compiled (see
    cpp_lexer.blank_inactive_branches()). `is_desktop_android` is the value of
    BUILDFLAG(IS_DESKTOP_ANDROID) in the conditions of default states.
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
                is_desktop_android,
            )
        )
    return definitions


def parse_param_definitions(
    path: str, code: str, structure: str, is_desktop_android: bool
) -> List[models.ParamDefinition]:
    """Finds param definitions such as BASE_FEATURE_PARAM(int, kFoo, ...) in a
    file. A definition is any BASE_FEATURE_PARAM call whose second argument is
    a kCamelCase identifier. `code` and `structure` are the views of the file
    (see cpp_lexer.lex()) without the code that is not compiled (see
    cpp_lexer.blank_inactive_branches()). `is_desktop_android` is the value of
    BUILDFLAG(IS_DESKTOP_ANDROID) in the conditions of default values.
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
                path, call.line, call.namespace, call.args, is_desktop_android
            )
        )
    return definitions

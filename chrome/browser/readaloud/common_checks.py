# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common presubmit checks for ReadAloud code.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

PRESUBMIT_VERSION = '2.0.0'

_ASSERTION_MACROS = (
    'EXPECT_EQ',
    'EXPECT_NE',
    'EXPECT_LE',
    'EXPECT_LT',
    'EXPECT_GE',
    'EXPECT_GT',
    'EXPECT_FLOAT_EQ',
    'EXPECT_DOUBLE_EQ',
    'EXPECT_NEAR',
    'EXPECT_STREQ',
    'EXPECT_STRNE',
    'EXPECT_STRCASEEQ',
    'EXPECT_STRCASENE',
    'ASSERT_EQ',
    'ASSERT_NE',
    'ASSERT_LE',
    'ASSERT_LT',
    'ASSERT_GE',
    'ASSERT_GT',
    'ASSERT_FLOAT_EQ',
    'ASSERT_DOUBLE_EQ',
    'ASSERT_NEAR',
    'ASSERT_STREQ',
    'ASSERT_STRNE',
    'ASSERT_STRCASEEQ',
    'ASSERT_STRCASENE',
)

_MACRO_RE = re.compile(
    r'\b(?P<macro>' + '|'.join(_ASSERTION_MACROS) + r')\s*\('
)

_NUMERIC_RE = re.compile(
    r'^[+-]?(?:0x[0-9a-fA-F]+|0b[01]+|\d+(?:\.\d*)?'
    r'(?:[eE][+-]?\d+)?)[uUlLfFzZ]*$'
)

_DURATION_RE = re.compile(
    r'^base::(?:Days|Hours|Minutes|Seconds|Milliseconds|'
    r'Microseconds|Nanoseconds)\(\s*[-+]?\d+(?:\.\d*)?[fF]?\s*\)$'
)

_STRING_RE = re.compile(r'^(?:u8|u|U|L)?\"(?:\\.|[^\"\\])*\"$', re.DOTALL)
_CHAR_RE = re.compile(r'^(?:u8|u|U|L)?\'(?:\\.|[^\'\\])*\'$')
_K_CONST_RE = re.compile(r'^(?:[A-Za-z_][A-Za-z0-9_]*::)*k[A-Z][A-Za-z0-9_]*$')
_SCOPED_ENUM_RE = re.compile(
    r'^(?:[A-Za-z_][A-Za-z0-9_]*::)+[A-Z][A-Z0-9_]{1,}$'
)
_ALL_CAPS_RE = re.compile(r'^[A-Z][A-Z0-9_]{2,}$')


def _ExtractMacroArgs(content, start_pos):
    """Extracts top-level comma-separated arguments starting from start_pos.

    start_pos is expected to be immediately after the opening '('.

    Returns:
      (args, end_pos) where args is a list of strings and end_pos is the index
      immediately after the matching closing ')', or (None, None) on error.
    """
    args = []
    current = []
    depth = 0
    in_string = False
    in_char = False
    in_single_comment = False
    in_multi_comment = False
    escape = False
    i = start_pos
    n = len(content)

    while i < n:
        c = content[i]
        next_c = content[i + 1] if i + 1 < n else ''

        if in_single_comment:
            if c == '\n':
                in_single_comment = False
            i += 1
            continue

        if in_multi_comment:
            if c == '*' and next_c == '/':
                in_multi_comment = False
                i += 2
                continue
            i += 1
            continue

        if escape:
            current.append(c)
            escape = False
            i += 1
            continue

        if c == '\\' and (in_string or in_char):
            current.append(c)
            escape = True
            i += 1
            continue

        if in_string:
            current.append(c)
            if c == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            current.append(c)
            if c == "'":
                in_char = False
            i += 1
            continue

        # Not in string, char, or comment
        if c == '/' and next_c == '/':
            in_single_comment = True
            i += 2
            continue

        if c == '/' and next_c == '*':
            in_multi_comment = True
            i += 2
            continue

        if c == '"':
            in_string = True
            current.append(c)
            i += 1
            continue

        if c == "'":
            in_char = True
            current.append(c)
            i += 1
            continue

        if c in '([{':
            depth += 1
            current.append(c)
            i += 1
            continue

        if c in ')]}':
            if depth == 0:
                if c == ')':
                    args.append(''.join(current).strip())
                    return args, i + 1
                return None, None
            depth -= 1
            current.append(c)
            i += 1
            continue

        if c == ',' and depth == 0:
            args.append(''.join(current).strip())
            current = []
            i += 1
            continue

        current.append(c)
        i += 1

    return None, None


def _CleanExpr(expr):
    """Removes C/C++ comments and surrounding whitespace."""
    expr = re.sub(r'/\*.*?\*/', '', expr, flags=re.DOTALL)
    expr = re.sub(r'//.*', '', expr)
    expr = expr.strip()
    while expr.startswith('(') and expr.endswith(')'):
        expr = expr[1:-1].strip()
    return expr


def _IsExpectedValue(expr):
    """Returns True if expr has strong evidence of being an expected value."""
    expr = _CleanExpr(expr)
    if not expr:
        return False

    # Literal null / boolean
    if expr in ('nullptr', 'NULL', 'true', 'false'):
        return True

    # Numeric literals: 0, 1, 0u, 100LL, 0x1a, 3.14f, etc.
    if _NUMERIC_RE.match(expr):
        return True

    # String literals: "...", u"...", etc.
    if _STRING_RE.match(expr):
        return True

    # Character literals: 'a', '\0', etc.
    if _CHAR_RE.match(expr):
        return True

    # Sentinels and duration constructors
    if expr in (
        'base::TimeDelta()',
        'base::Time()',
        'std::nullopt',
        'std::string()',
        'std::u16string()',
    ):
        return True
    if _DURATION_RE.match(expr):
        return True

    # Google-style constants: kChannels, media::kSampleRate
    if _K_CONST_RE.match(expr):
        return True

    # All-caps constants or scoped enums: PlaybackMode::CLASSIC, STATUS_OK
    if _SCOPED_ENUM_RE.match(expr) or _ALL_CAPS_RE.match(expr):
        return True

    # Variable or expression containing 'expected' (case-insensitive)
    if re.search(r'(?:\b|_)expected(?:\b|_)', expr, re.IGNORECASE):
        return True

    return False


def _IsActualValue(expr):
    """Returns True if expr has strong evidence of being an actual value."""
    expr = _CleanExpr(expr)
    if not expr:
        return False
    if re.search(r'(?:\b|_)actual(?:\b|_)', expr, re.IGNORECASE):
        return True
    if re.search(r'(?:\b|_)got(?:\b|_)', expr, re.IGNORECASE):
        return True
    return False


def _IsAssertionOrderViolation(arg1, arg2):
    """Returns True if arg1 is expected and arg2 is actual."""
    if _IsActualValue(arg1):
        return False
    if _IsActualValue(arg2) and not _IsActualValue(arg1):
        return True

    arg1_expected = _IsExpectedValue(arg1)
    arg2_expected = _IsExpectedValue(arg2)

    # Flag when arg1 is expected and arg2 is NOT expected.
    # If both or neither are expected, do not flag.
    return arg1_expected and not arg2_expected


def CheckTestAssertionOrder(input_api, output_api):
    """Checks that test expectations are in the format (actual, expected)."""
    violations = []

    def is_cpp_test_file(path):
        return path.endswith(('.cc', '.cpp', '.h', '.mm'))

    for f in input_api.AffectedFiles(include_deletes=False):
        local_path = f.LocalPath()
        if not is_cpp_test_file(local_path):
            continue

        changed_lines = {ln for ln, _ in f.ChangedContents()}
        if not changed_lines:
            continue

        new_lines = f.NewContents()
        contents = '\n'.join(new_lines)

        line_starts = [0]
        for line in new_lines[:-1]:
            line_starts.append(line_starts[-1] + len(line) + 1)

        def offset_to_line(offset):
            lo, hi = 0, len(line_starts) - 1
            while lo < hi:
                mid = (lo + hi + 1) // 2
                if line_starts[mid] <= offset:
                    lo = mid
                else:
                    hi = mid - 1
            return lo + 1

        for match in _MACRO_RE.finditer(contents):
            macro_name = match.group('macro')
            start_offset = match.start()
            args_start_offset = match.end()

            args, end_offset = _ExtractMacroArgs(contents, args_start_offset)
            if not args or len(args) < 2:
                continue

            start_line = offset_to_line(start_offset)
            end_line = offset_to_line(end_offset - 1)
            covered_lines = range(start_line, end_line + 1)

            # Only flag if at least one line of the macro call was changed.
            if not any(ln in changed_lines for ln in covered_lines):
                continue

            # Support '// nocheck' escape hatch on any covered line.
            if any(
                '// nocheck' in new_lines[ln - 1]
                for ln in covered_lines
                if 1 <= ln <= len(new_lines)
            ):
                continue

            # Skip if the invocation line starts with a comment.
            if new_lines[start_line - 1].strip().startswith('//'):
                continue

            arg1, arg2 = args[0], args[1]
            if _IsAssertionOrderViolation(arg1, arg2):
                snippet = f'{macro_name}({", ".join(args)})'
                if len(snippet) > 80:
                    snippet = snippet[:77] + '...'
                violations.append(f'  {local_path}:{start_line}: {snippet}')

    if not violations:
        return []

    return [
        output_api.PresubmitPromptWarning(
            'Test assertions and expectations should follow the format '
            '<actual>, <expected> rather than <expected>, <actual>.\n'
            'See go/noyoda for guidelines.\n'
            'Violations found:\n' + '\n'.join(violations)
        )
    ]


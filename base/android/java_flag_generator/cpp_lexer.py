# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Minimal C++ lexing and preprocessing helpers."""

import re
from typing import Dict, List, Optional, Tuple

_SPECIAL_RE = re.compile(r'//|/\*|"|\'')
_NON_NEWLINE_RE = re.compile(r'[^\n]')
_RAW_STRING_DELIMITER_RE = re.compile(r'([^()\\\s]{0,16})\(')

_OPEN_TO_CLOSE = {'(': ')', '[': ']', '{': '}'}
_CLOSERS = set(_OPEN_TO_CLOSE.values())


def _blank(segment: str) -> str:
    return _NON_NEWLINE_RE.sub(' ', segment)


def _is_word_char(c: str) -> bool:
    return c.isalnum() or c == '_'


def _is_raw_string_quote(text: str, quote_index: int) -> bool:
    """Whether the `"` at quote_index opens a C++11 raw string literal
    R"(...)", u8R"(...)", LR"(...)", etc.
    """
    p = quote_index - 1
    if p < 0 or text[p] != 'R':
        return False
    if p >= 2 and text[p - 2 : p] == 'u8':
        p -= 2
    elif p >= 1 and text[p - 1] in 'uUL':
        p -= 1
    return p == 0 or not _is_word_char(text[p - 1])


def _is_digit_separator(text: str, quote_index: int) -> bool:
    """Whether the `'` at quote_index is a C++14 digit separator (100'000)."""
    j = quote_index - 1
    while j >= 0 and _is_word_char(text[j]):
        j -= 1
    token = text[j + 1 : quote_index]
    return bool(token) and token[0].isdigit()


def _find_quoted_literal_end(text: str, start: int, quote: str) -> int:
    """Returns the index of the closing quote (or of the newline / EOF)."""
    j = start
    n = len(text)
    while j < n:
        c = text[j]
        if c == '\\':
            j += 2
            continue
        if c == quote or c == '\n':
            return j
        j += 1
    return n


def lex(text: str) -> Tuple[str, str]:
    """Returns two views of a file, both the same length as the input so that
    indices (and therefore line numbers) line up:
    * code: Comments replaced by spaces. String literals are kept, so their
      values can be read.
    * structure: Comments and the contents of string/char literals replaced by
      spaces. Used for anything that scans for braces, parentheses, or commas,
      which must not be confused by e.g. "(" inside a string.
    """
    code_parts = []
    structure_parts = []
    i = 0
    n = len(text)
    while i < n:
        m = _SPECIAL_RE.search(text, i)
        if not m:
            code_parts.append(text[i:])
            structure_parts.append(text[i:])
            break
        start = m.start()
        code_parts.append(text[i:start])
        structure_parts.append(text[i:start])
        token = m.group()

        if token == '//':
            end = text.find('\n', start)
            end = n if end == -1 else end
            blanked = _blank(text[start:end])
            code_parts.append(blanked)
            structure_parts.append(blanked)
            i = end
        elif token == '/*':
            end = text.find('*/', start + 2)
            end = n if end == -1 else end + 2
            blanked = _blank(text[start:end])
            code_parts.append(blanked)
            structure_parts.append(blanked)
            i = end
        elif token == '"' and _is_raw_string_quote(text, start):
            dm = _RAW_STRING_DELIMITER_RE.match(text, start + 1)
            if not dm:
                # Not a valid raw string; treat the quote as ordinary text.
                code_parts.append(token)
                structure_parts.append(token)
                i = start + 1
                continue
            body_start = dm.end()
            end_marker = ')' + dm.group(1) + '"'
            body_end = text.find(end_marker, body_start)
            body_end = n if body_end == -1 else body_end
            end = min(n, body_end + len(end_marker))
            code_parts.append(text[start:end])
            structure_parts.append(
                text[start:body_start]
                + _blank(text[body_start:body_end])
                + text[body_end:end]
            )
            i = end
        elif token == "'" and _is_digit_separator(text, start):
            code_parts.append(token)
            structure_parts.append(token)
            i = start + 1
        else:
            # The `"` or `'` opens a regular string literal
            close = _find_quoted_literal_end(text, start + 1, token)
            terminated = close < n and text[close] == token
            end = close + 1 if terminated else close
            code_parts.append(text[start:end])
            structure_parts.append(
                token
                + _blank(text[start + 1 : close])
                + (token if terminated else '')
            )
            i = end
    return ''.join(code_parts), ''.join(structure_parts)


def find_matching(structure: str, open_index: int) -> int:
    """Returns the index of the bracket closing the one at open_index.
    Raises ValueError if the brackets are unbalanced.
    """
    stack = []
    for j in range(open_index, len(structure)):
        c = structure[j]
        if c in _OPEN_TO_CLOSE:
            stack.append(_OPEN_TO_CLOSE[c])
        elif c in _CLOSERS:
            if not stack or stack.pop() != c:
                raise ValueError(f'Unbalanced "{c}"')
            if not stack:
                return j
    raise ValueError('Unterminated bracket')


def split_top_level(
    structure: str, start: int, end: int
) -> List[Tuple[int, int]]:
    """Splits structure[start:end] at commas not nested in any bracket."""
    ranges = []
    depth = 0
    piece_start = start
    for j in range(start, end):
        c = structure[j]
        if c in _OPEN_TO_CLOSE:
            depth += 1
        elif c in _CLOSERS:
            depth -= 1
        elif c == ',' and depth == 0:
            ranges.append((piece_start, j))
            piece_start = j + 1
    ranges.append((piece_start, end))
    return ranges


# The BUILDFLAG()s for the target OS in //build/build_config.h, and their values
# in Android builds.
_ANDROID_OS_BUILDFLAGS = {
    'IS_AIX': False,
    'IS_ANDROID': True,
    'IS_APPLE': False,
    'IS_ASMJS': False,
    'IS_BSD': False,
    'IS_CHROMEOS': False,
    'IS_FREEBSD': False,
    'IS_FUCHSIA': False,
    'IS_IOS': False,
    'IS_IOS_MACCATALYST': False,
    'IS_IOS_TVOS': False,
    'IS_LINUX': False,
    'IS_MAC': False,
    'IS_NETBSD': False,
    'IS_OPENBSD': False,
    'IS_OZONE': False,
    'IS_POSIX': True,
    'IS_QNX': False,
    'IS_SOLARIS': False,
    'IS_WATCHOS': False,
    'IS_WIN': False,
}
_DIRECTIVE_NAME_RE = re.compile(r'\s*#\s*(\w*)')
_CONDITION_TOKEN_RE = re.compile(r'&&|\|\||\w+|\S')
# Tokens that end an unknown operand, such as defined(FOO) or FOO >= 2, unless
# they are nested in parentheses.
_OPERAND_END_TOKENS = ('&&', '||', '?', ':', ',', ')')
# The directives that start a branch of an #if block.
_BRANCH_DIRECTIVES = (
    'if',
    'ifdef',
    'ifndef',
    'elif',
    'elifdef',
    'elifndef',
    'else',
)

# The value of a condition: True, False, or None if it is unknown.
_Value = Optional[bool]


class _UnsupportedCondition(Exception):
    pass


class _ConditionEvaluator:
    """Evaluates the condition of an #if or #elif.

    Only the BUILDFLAG()s in `buildflags` are known. Other operands, such as
    other BUILDFLAG()s or defined(FOO), are unknown, and follow three-valued
    logic: false && x is false and true || x is true, but any other
    combination with an unknown operand is unknown, e.g.
    BUILDFLAG(IS_WIN) || BUILDFLAG(FOO). Conditions that are unknown as a whole
    are assumed to be true, even when negated.
    """

    def __init__(self, condition: str, buildflags: Dict[str, bool]):
        self._tokens = _CONDITION_TOKEN_RE.findall(condition)
        self._pos = 0
        self._buildflags = buildflags

    def evaluate(self) -> bool:
        try:
            value = self._parse_or()
            # E.g. the "== 0" of "BUILDFLAG(IS_WIN) == 0" is not supported.
            if self._pos != len(self._tokens):
                value = None
        except _UnsupportedCondition:
            value = None
        return True if value is None else value

    def _peek(self) -> Optional[str]:
        if self._pos < len(self._tokens):
            return self._tokens[self._pos]
        return None

    def _next(self) -> str:
        token = self._peek()
        if token is None:
            raise _UnsupportedCondition()
        self._pos += 1
        return token

    def _expect(self, expected: str) -> None:
        if self._next() != expected:
            raise _UnsupportedCondition()

    def _and(self, a: _Value, b: _Value) -> _Value:
        if a is False or b is False:
            return False
        if a is None or b is None:
            return None
        return True

    def _or(self, a: _Value, b: _Value) -> _Value:
        if a is True or b is True:
            return True
        if a is None or b is None:
            return None
        return False

    def _parse_or(self) -> _Value:
        value = self._parse_and()
        while self._peek() == '||':
            self._pos += 1
            value = self._or(value, self._parse_and())
        return value

    def _parse_and(self) -> _Value:
        value = self._parse_not()
        while self._peek() == '&&':
            self._pos += 1
            value = self._and(value, self._parse_not())
        return value

    def _parse_not(self) -> _Value:
        if self._peek() == '!':
            self._pos += 1
            value = self._parse_not()
            return None if value is None else not value
        return self._parse_operand()

    def _parse_operand(self) -> _Value:
        if self._peek() == '(':
            self._pos += 1
            value = self._parse_or()
            self._expect(')')
            return value
        if self._tokens[self._pos : self._pos + 2] == ['BUILDFLAG', '(']:
            self._pos += 2
            flag = self._next()
            self._expect(')')
            return self._buildflags.get(flag)
        # Any other operand is unknown.
        start = self._pos
        depth = 0
        while (token := self._peek()) is not None:
            if depth == 0 and token in _OPERAND_END_TOKENS:
                break
            if token == '(':
                depth += 1
            elif token == ')':
                depth -= 1
            self._pos += 1
        if self._pos == start:
            raise _UnsupportedCondition()
        return None


class _Block:
    """The state of an #if block, from its #if to its #endif."""

    def __init__(self, parent_active: bool):
        # Whether the code around the block is compiled.
        self._parent_active = parent_active
        # Whether an earlier branch is compiled.
        self._taken = False
        # Whether the current branch is compiled.
        self.active = False

    def start_branch(self, value: bool) -> None:
        """Starts the branch of an #if, #elif or #else whose condition has
        `value`.
        """
        self.active = self._parent_active and not self._taken and value
        self._taken = self._taken or self.active


def blank_inactive_branches(
    text: str, code: str, structure: str, is_desktop_android: bool
) -> Tuple[str, str, str]:
    """Blanks the branches of #if blocks that an Android build doesn't
    compile, in the text, code and structure views of a file (see lex()), and
    returns the resulting views. The same lines are blanked in all three.

    The conditions of #if and #elif are evaluated with these values:
    * The OS BUILDFLAG()s of //build/build_config.h have their Android values,
      e.g. BUILDFLAG(IS_ANDROID) is true and BUILDFLAG(IS_WIN) is false.
    * BUILDFLAG(IS_DESKTOP_ANDROID) is `is_desktop_android`.
    * Anything else, such as BUILDFLAG(FOO) or defined(FOO), is unknown. Known
      values only decide a condition if unknown ones can't change it, e.g.
      BUILDFLAG(IS_WIN) && defined(FOO) is false. Conditions that are unknown
      as a whole, e.g. BUILDFLAG(IS_WIN) || BUILDFLAG(FOO), are assumed to be
      true, even when negated, and so are the conditions of #ifdef and
      #ifndef.
    The lines of #if, #elif, #else and #endif are blanked too, because they
    may sit between the arguments of a macro, e.g.
      BASE_FEATURE(kFoo,
      #if BUILDFLAG(IS_DESKTOP_ANDROID)
                   base::FEATURE_ENABLED_BY_DEFAULT);
      #else
                   base::FEATURE_DISABLED_BY_DEFAULT);
      #endif
    Other directives are left alone, unless they are in a blanked branch.
    `structure` decides which lines are directives, so that a `#` inside a
    string literal or comment is never mistaken for one.
    """
    buildflags = dict(
        _ANDROID_OS_BUILDFLAGS, IS_DESKTOP_ANDROID=is_desktop_android
    )
    text_lines = text.split('\n')
    code_lines = code.split('\n')
    structure_lines = structure.split('\n')
    num_lines = len(structure_lines)
    keep = [True] * num_lines
    blocks: List[_Block] = []
    idx = 0

    def strip_continuation(line: str) -> str:
        line = line.rstrip()
        return line[:-1] if line.endswith('\\') else line

    while idx < num_lines:
        active = not blocks or blocks[-1].active
        m = _DIRECTIVE_NAME_RE.match(structure_lines[idx])
        if not m:
            keep[idx] = active
            idx += 1
            continue
        end = idx
        while end + 1 < num_lines and code_lines[end].rstrip().endswith('\\'):
            end += 1
        name = m.group(1)
        if name in ('if', 'ifdef', 'ifndef'):
            blocks.append(_Block(active))
        if name in _BRANCH_DIRECTIVES and blocks:
            # The conditions of #ifdef, #ifndef, #elifdef and #elifndef are
            # unknown, so they are assumed to be true, like #else.
            value = True
            if name in ('if', 'elif'):
                condition = ' '.join(
                    strip_continuation(line)
                    for line in [structure_lines[idx][m.end() :]]
                    + structure_lines[idx + 1 : end + 1]
                )
                value = _ConditionEvaluator(condition, buildflags).evaluate()
            blocks[-1].start_branch(value)
            keep_directive = False
        elif name == 'endif' and blocks:
            blocks.pop()
            keep_directive = False
        else:
            # Other directives, and #elif, #else and #endif without an #if.
            keep_directive = active
        keep[idx : end + 1] = [keep_directive] * (end + 1 - idx)
        idx = end + 1

    def blank(lines: List[str]) -> str:
        return '\n'.join(
            line if kept else ' ' * len(line) for line, kept in zip(lines, keep)
        )

    return blank(text_lines), blank(code_lines), blank(structure_lines)

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Minimal C++ lexing helpers."""

import re
from typing import List, Tuple

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
    if p >= 2 and text[p - 2:p] == 'u8':
        p -= 2
    elif p >= 1 and text[p - 1] in 'uUL':
        p -= 1
    return p == 0 or not _is_word_char(text[p - 1])


def _is_digit_separator(text: str, quote_index: int) -> bool:
    """Whether the `'` at quote_index is a C++14 digit separator (100'000)."""
    j = quote_index - 1
    while j >= 0 and _is_word_char(text[j]):
        j -= 1
    token = text[j + 1:quote_index]
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
            structure_parts.append(text[start:body_start] +
                                   _blank(text[body_start:body_end]) +
                                   text[body_end:end])
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
            structure_parts.append(token + _blank(text[start + 1:close]) +
                                   (token if terminated else ''))
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


def split_top_level(structure: str, start: int,
                    end: int) -> List[Tuple[int, int]]:
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


def blank_preprocessor_lines(code: str, structure: str) -> Tuple[str, str]:
    """Blanks preprocessor directive lines (incl. continuations) in both views.
    `structure` decides which lines are directives so that a `#` inside a string
    literal or comment is never mistaken for one.
    """
    code_lines = code.split('\n')
    structure_lines = structure.split('\n')
    in_continuation = False
    for idx, line in enumerate(structure_lines):
        if in_continuation or line.lstrip().startswith('#'):
            in_continuation = code_lines[idx].rstrip().endswith('\\')
            code_lines[idx] = ' ' * len(code_lines[idx])
            structure_lines[idx] = ' ' * len(line)
    return '\n'.join(code_lines), '\n'.join(structure_lines)

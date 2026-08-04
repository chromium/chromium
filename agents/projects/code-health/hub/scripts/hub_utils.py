# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utility functions for Code Health Hub plugins."""
# pylint: disable=line-too-long

import re


def strip_comments(text: str) -> str:
    """Removes Java-style line (//) and block (/* */) comments from text."""
    # Strip block comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Strip line comments
    text = re.sub(r'//.*$', '', text, flags=re.MULTILINE)
    return text


def strip_strings(text: str) -> str:
    """Removes double-quoted string literals from text, handling escaped quotes."""
    return re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)


def extract_method_body(content, start_pos):
    """Finds matching braces to extract a method body block starting at start_pos."""
    brace_start = content.find('{', start_pos)
    if brace_start == -1:
        return None, -1

    count = 1
    idx = brace_start + 1
    while idx < len(content) and count > 0:
        if content[idx] == '{':
            count += 1
        elif content[idx] == '}':
            count -= 1
        idx += 1

    if count == 0:
        return content[brace_start:idx], brace_start
    return None, -1


def strip_annotations_preserve_length(text: str) -> str:
    """Replaces Java annotations (like @Override) with spaces to preserve character index offsets."""

    def repl(match):
        return ' ' * len(match.group(0))

    return re.sub(r'@[a-zA-Z0-9_.]+(\([^)]*\))?', repl, text)

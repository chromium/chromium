# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utility functions for Code Health Hub plugins."""

import os
import re
import subprocess


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


def run_cmd(cmd, cwd=None):
    """Runs a command and returns (returncode, output).

    On success, output is stdout only so that stderr warnings never pollute
    parsed output. On failure, output is the non-empty stdout and stderr
    joined by a newline. If the command cannot be run, returns 127.
    """
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
    except OSError as e:
        return 127, f'Failed to run {cmd[0]}: {e}'
    if result.returncode == 0:
        return 0, result.stdout or ''
    parts = ((result.stdout or '').strip(), (result.stderr or '').strip())
    return result.returncode, '\n'.join(p for p in parts if p)


def find_repo_root(start=None):
    """Returns the git repo root containing |start|, or |start| on failure."""
    base = start or os.getcwd()
    rc, out = run_cmd(['git', 'rev-parse', '--show-toplevel'], cwd=base)
    if rc == 0 and out.strip():
        return out.strip().splitlines()[0]
    return base

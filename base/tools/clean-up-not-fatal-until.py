#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Removes the base::NotFatalUntil milestones up to and including M.

Usage: base/tools/clean-up-not-fatal-until.py -m <M>

For each milestone <= M in base/not_fatal_until.h, this removes:
- Its entry in base/not_fatal_until.h.
- Its arguments: CHECK(cond, base::NotFatalUntil::M1) -> CHECK(cond)
- The now unreachable code after the resulting NOTREACHED() and CHECK(false).
- The base/not_fatal_until.h includes that are no longer needed.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess

GIT = 'git.exe' if os.name == 'nt' else 'git'
HEADER = Path('base/not_fatal_until.h')
HEADER_ENTRY = re.compile(r'^ *M(\d+) = \d+,\n', re.MULTILINE)


def remove_header_entries(header, milestone):
    """Returns `header` without entries <= `milestone`, and their numbers."""
    removed = [m for m in HEADER_ENTRY.findall(header) if int(m) <= milestone]
    header = HEADER_ENTRY.sub(
        lambda entry: '' if entry.group(1) in removed else entry.group(0),
        header)
    return header, removed


def remove_unreachable_code(lines, i):
    """Removes the code after the statement at `lines[i]`, until its block end.

    Relies on clang-format's indentation: the statement's continuation lines
    are more indented than `lines[i]`, the next statements in the block are
    indented at least as much, and the block end (closing brace, next case
    label, or preprocessor directive) is less indented.
    """

    def indent(line):
        return len(line) - len(line.lstrip(' '))

    begin = i + 1
    while begin < len(lines) and indent(lines[begin]) > indent(lines[i]):
        begin += 1
    end = begin
    while end < len(lines) and (lines[end].isspace()
                                or indent(lines[end]) >= indent(lines[i])):
        end += 1
    while end > begin and re.match(r'\s*(//|$)', lines[end - 1]):
        end -= 1  # Keep the blank lines and comments before the block end.
    del lines[begin:end]


def clean_up(contents, milestones):
    """Returns `contents` without the NotFatalUntil `milestones` arguments."""
    arg = r'(?:base::)?NotFatalUntil::M(?:%s)\b' % '|'.join(milestones)

    # Once fatal, NOTREACHED() and CHECK(false) never return. The code after
    # them would fail to compile with -Wunreachable-code-aggressive.
    lines = contents.splitlines(keepends=True)
    for i in reversed(range(len(lines))):
        if re.match(rf'\s*(NOTREACHED\(|CHECK\(false,)\s*{arg}\s*\)', lines[i]):
            remove_unreachable_code(lines, i)
    contents = ''.join(lines)

    # CHECK(cond, base::NotFatalUntil::M1) -> CHECK(cond)
    contents = re.sub(rf',\s*{arg}\s*\)', ')', contents)
    # NOTREACHED(base::NotFatalUntil::M1) -> NOTREACHED()
    contents = re.sub(rf'\(\s*{arg}\s*\)', '()', contents)

    if 'NotFatalUntil' not in contents:
        contents = contents.replace('#include "base/not_fatal_until.h"\n', '')
    return contents


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-m', '--milestone', type=int, required=True)
    milestone = parser.parse_args().milestone

    assert Path.cwd() == Path(__file__).resolve().parents[2], (
        'Must be run from the src/ directory.')
    header, milestones = remove_header_entries(
        HEADER.read_text(encoding='utf-8'), milestone)
    if not milestones:
        print(f'No milestones <= M{milestone} in {HEADER}.')
        return
    HEADER.write_text(header, encoding='utf-8', newline='\n')

    pattern = r'NotFatalUntil::M(%s)\b' % '|'.join(milestones)
    files = subprocess.check_output(
        [GIT, 'grep', '-lE', pattern, '--', '*.cc', '*.h', '*.mm'],
        text=True).splitlines()
    for file in files:
        path = Path(file)
        contents = clean_up(path.read_text(encoding='utf-8'), milestones)
        path.write_text(contents, encoding='utf-8', newline='\n')
        if re.search(pattern, contents):
            print(f'{file}: Needs a manual clean up.')
    print(f'Removed M{milestones[0]} to M{milestones[-1]} from {len(files)} '
          'files.')

    subprocess.check_call([GIT, 'cl', 'format'])


if __name__ == '__main__':
    main()

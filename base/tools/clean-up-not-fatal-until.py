#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import subprocess
import sys


def main(args):
    parser = argparse.ArgumentParser(
        description='Removes usage of obsolete base::NotFatalUntil::M<N> values'
    )
    parser.add_argument('-m', '--milestone', required=True)
    parsed_args = parser.parse_args(args=args)

    try:
        milestone = int(parsed_args.milestone)
    except ValueError:
        print(f'--milestone must be a number: {parsed_args.milestone}',
              file=sys.stderr)
        return -1

    pattern = f'NotFatalUntil::M{milestone}'
    print(f'Searching for files with {pattern}...', file=sys.stderr)
    files = subprocess.check_output(
        ('git.exe' if os.name == 'nt' else 'git', 'gs', pattern,
         '--name-only')).decode('utf-8').splitlines()

    print(f'Found {len(files)} {"file" if len(files) == 1 else "files"}',
          file=sys.stderr)

    # Intended to match base::NotFatalUntil as the last argument of CHECK(),
    # CHECK_EQ(), et cetera. While this could match more things than expected,
    # as it only looks for a comma + whitespace + a base::NotFatalUntil value,
    # in practice, it more or less works.
    replace_regex = re.compile(fr',\s+?(?:base::)?{pattern}')
    # Macros that optionally take a single base::NotFatalUntil argument need a
    # separate regex.
    macro_replace_regex = re.compile(
        fr'(?P<macro>CHECK_IS_(?:NOT_)?TEST|NOTREACHED)\((?:base::)?{pattern}\)'
    )
    for f in files:
        print(f'Updating {f}...', file=sys.stderr)
        with open(f, 'r+') as f:
            contents = f.read()
            new_contents = replace_regex.sub('', contents)
            new_contents = macro_replace_regex.sub(f'\g<macro>()',
                                                   new_contents)
            if 'NotFatalUntil' not in new_contents:
                new_contents = new_contents.replace(
                    '#include "base/not_fatal_until.h"\n', '')
            f.seek(0)
            f.truncate()
            f.write(new_contents)
    print(f'Done!', file=sys.stderr)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

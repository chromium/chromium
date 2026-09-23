#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates the response file used by the `allow_all_warnings` GN config.

The generated file contains one `-A<lint>` entry for every lint known to
`rustc`.  This script is typically invoked using the
`//build/rust/gni_impl:gen_allow_all_warnings_rsp` target.

See `config("allow_all_warnings")` in `//build/rust/gni_impl/BUILD.gn` for
explanation 1) why the individual lints (rather than just `-Awarnings`) are
needed, and 2) why we don't just use `--cap-lints=allow`.
"""

import argparse
import os
import subprocess
import sys

sys.path.append(os.path.dirname(__file__))
from rustc_print_cfg import rustc_name

# Lints that must *not* be listed in the generated file, because their level is
# controlled elsewhere.
_LINTS_CONTROLLED_ELSEWHERE = [
    # Controlled by the `//build/rust:forbid_unsafe` config (which is applied
    # based on the `allow_unsafe` GN arg of `cargo_crate` and friends).
    'unsafe_code',
]

# The levels that `rustc -W help` reports in the 2nd column of the table of
# lints.  (The table of lint *groups* has no such column - this is how the two
# tables are told apart.)
_LINT_LEVELS = ['allow', 'warn', 'deny', 'forbid']


def parse_lints(rustc_w_help_output):
    """Extracts lint names from the output of `rustc -W help`.

    Returns a sorted list of the lints that the `.rsp` file should cover.
    """
    lints = set()
    for line in rustc_w_help_output.splitlines():
        # Rows of the table of lints look like this:
        #     <lint-name>  <lint-level>  <description...>
        # Everything else (e.g. the table of lint groups, the table headers,
        # and the free-form text) has no lint level in the 2nd column.
        fields = line.split()
        if len(fields) >= 2 and fields[1] in _LINT_LEVELS:
            lints.add(fields[0].replace('-', '_'))

    if not lints:
        raise ValueError('Failed to parse the output of `rustc -W help`')

    return sorted(lints.difference(_LINTS_CONTROLLED_ELSEWHERE))


def generate_rsp_contents(rustc_w_help_output):
    # Note that `rustc` response files don't support comments - the generated
    # file can only contain command-line arguments.
    return ''.join(f'-A{lint}\n' for lint in parse_lints(rustc_w_help_output))


def main():
    parser = argparse.ArgumentParser('gen_allow_all_warnings_rsp.py')
    parser.add_argument('--rust-prefix', required=True, help='rust path prefix')
    parser.add_argument('--output-path', required=True, help='output file')
    args = parser.parse_args()

    rustc_path = os.path.join(args.rust_prefix, rustc_name())
    rustc_w_help_output = subprocess.run(
        [rustc_path, '-W', 'help'],
        stdout=subprocess.PIPE,
        check=True,
        text=True,
    ).stdout

    os.makedirs(os.path.dirname(args.output_path), exist_ok=True)
    with open(args.output_path, 'w', newline='\n') as output_file:
        output_file.write(generate_rsp_contents(rustc_w_help_output))
    return 0


if __name__ == '__main__':
    sys.exit(main())

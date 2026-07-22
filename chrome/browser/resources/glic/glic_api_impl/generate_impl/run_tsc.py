#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import subprocess
"""
Runs the typescript compiler, and writes a stamp file.
"""


def Main():
    if len(sys.argv) < 4:
        print(
            "Usage: run_tsc.py <stamp_file> <node_py> <tsc_js> [tsc_args...]",
            file=sys.stderr)
        sys.exit(1)

    stamp_file = sys.argv[1]
    node_py = sys.argv[2]
    tsc_js = sys.argv[3]
    tsc_args = sys.argv[4:]

    cmd = [sys.executable, node_py, tsc_js] + tsc_args

    result = subprocess.run(cmd)
    if result.returncode != 0:
        sys.exit(result.returncode)

    os.makedirs(os.path.dirname(stamp_file), exist_ok=True)
    with open(stamp_file, 'w') as f:
        f.write('ok\n')


if __name__ == '__main__':
    Main()

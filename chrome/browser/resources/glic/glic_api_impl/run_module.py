#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generic helper script to run a python module from GN."""

import os
import subprocess
import sys


def Main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <source_root> <module_name> [args...]",
              file=sys.stderr)
        sys.exit(1)

    source_dir = os.path.abspath(sys.argv[1])
    module_name = sys.argv[2]
    module_args = sys.argv[3:]

    env = os.environ.copy()
    paths = env.get('PYTHONPATH', '').split(os.path.pathsep)
    paths = [source_dir] + paths
    env['PYTHONPATH'] = os.path.pathsep.join(paths)

    sys.exit(
        subprocess.call([sys.executable, '-m', module_name] + module_args,
                        env=env))


if __name__ == '__main__':
    Main()

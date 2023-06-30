# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

from cca import build
from cca import cli
from cca import util


@cli.command(
    "tsc",
    help="check code with tsc",
    description="Check types with tsc. "
    "Please build Chrome at least once before running the command.",
)
@cli.option("board")
def cmd(board: str) -> int:
    build.generate_tsconfig(board)

    try:
        util.run_node(["typescript/bin/tsc"])
    except subprocess.CalledProcessError as e:
        print("TypeScript check failed, return code =", e.returncode)
        return e.returncode

    return 0

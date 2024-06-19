# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import subprocess

from cra import build
from cra import cli
from cra import util


@cli.command(
    "tsc",
    help="check code with tsc",
    description="Check types with tsc. "
    "Please build Chrome at least once before running the command.",
)
@util.build_dir_option()
def cmd(build_dir: pathlib.Path) -> int:
    build.generate_tsconfig(build_dir)

    try:
        util.run_node(["typescript/bin/tsc"], cwd=util.get_cra_root())
    except subprocess.CalledProcessError as e:
        print("TypeScript check failed, return code =", e.returncode)
        return e.returncode

    return 0

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from cca import cli
from cca import util


@cli.command(
    "lint",
    help="check code with eslint",
    description="Check coding styles with eslint.",
)
@cli.option("--fix", action="store_true")
@cli.option("--eslintrc", help="use alternative eslintrc")
def cmd(fix: bool, eslintrc: str) -> int:
    cmd = [
        "eslint/bin/eslint.js",
        "js",
    ]
    if fix:
        cmd.append("--fix")
    eslintrc = eslintrc or os.path.join(
        util.get_chromium_root(), "tools/web_dev_style/eslint.config.mjs")
    cmd.extend(["--config", eslintrc])
    try:
        util.run_node(cmd)
    except subprocess.CalledProcessError as e:
        print("ESLint check failed, return code =", e.returncode)
        return e.returncode
    # TODO(pihsun): Add lit-analyzer to the check. It's not included in the
    # chrome source tree and can be manually installed with `npm install -g
    # lit-analyzer ts-lit-plugin`. Maybe this can be added as an optional check
    # for now?
    return 0

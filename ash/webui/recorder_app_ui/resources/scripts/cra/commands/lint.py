# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pathlib
import shutil
import subprocess
from typing import Optional

from cra import build
from cra import cli
from cra import util


def _check_eslint(fix: bool, eslintrc: Optional[str]) -> Optional[int]:
    # TODO(pihsun): Adjust the rules when build_dir is specified, so we can
    # take advantage of the full type information.
    cmd = [
        "eslint/bin/eslint.js",
        ".",
    ]
    if fix:
        cmd.append("--fix")
    if eslintrc is not None:
        cmd.extend(["--config", eslintrc])
    try:
        util.run_node(cmd, cwd=util.get_cra_root())
    except subprocess.CalledProcessError as e:
        logging.error("ESLint check failed, return code = %d", e.returncode)
        return e.returncode


def _check_lit_analyzer(build_dir: Optional[pathlib.Path]) -> Optional[int]:
    # lit-analyzer is not included in the chrome source tree since it adds ~8MB
    # to node_modules for the dependencies, and it also use `typescript/lib`
    # which adds another ~52MB of file size. It's quite significant consider
    # that it's not used by anything other than this, so this is added as an
    # optional check for now.
    if shutil.which("lit-analyzer") is None:
        logging.info(
            "lit-analyzer is not installed locally. For better lint checks "
            "for Lit please install it by `npm install lit-analyzer -g`.")
        return

    if build_dir is None:
        logging.info("--build_dir is required for lit-analyzer check.")
        return

    build.generate_tsconfig(build_dir)
    try:
        util.run(
            [
                "lit-analyzer",
                "--strict",
                # Checks that custom elements are added to type map in
                # HTMLElementTagNameMap.
                "--rules.no-missing-element-type-definition",
                "error",
                # The check only works when the properties are defined by
                # decorator, not when defined by `static properties`.
                "--rules.no-unknown-attribute",
                "off",
                # lit-analyzer currently use an old version of
                # vscode-css-languageservice, which doesn't recognize
                # @container that we use.
                # TODO(pihsun): Re-enable this after there's a new lit-analyzer
                # release that includes
                # https://github.com/runem/lit-analyzer/commit/294dfb5b94e914d1a29f64c9481ffe0290a90e77
                "--rules.no-invalid-css",
                "off",
                "**/*.ts",
            ],
            cwd=util.get_cra_root(),
        )
    except subprocess.CalledProcessError as e:
        logging.error("lit-analyzer check failed, return code = %d",
                      e.returncode)
        return e.returncode


@cli.command(
    "lint",
    help="check code with eslint",
    description="Check coding styles with eslint.",
)
@cli.option("--fix", action="store_true")
@cli.option("--eslintrc", help="use alternative eslintrc")
@util.build_dir_option(optional=True)
def cmd(fix: bool, eslintrc: Optional[str],
        build_dir: Optional[pathlib.Path]) -> int:
    eslint_ret = _check_eslint(fix, eslintrc)
    lit_analyzer_ret = _check_lit_analyzer(build_dir)
    # TODO(pihsun): Add stylelint check as an local only optional check similar
    # to lit-analyzer.

    return eslint_ret or lit_analyzer_ret or 0

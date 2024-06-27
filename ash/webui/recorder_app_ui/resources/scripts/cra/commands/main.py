# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from cra import cli
from cra.commands import add_strings
from cra.commands import bundle
from cra.commands import dev
from cra.commands import lint
from cra.commands import tsc


@cli.root(children=[
    add_strings.cmd,
    bundle.cmd,
    dev.cmd,
    lint.cmd,
    tsc.cmd,
])
@cli.option(
    "--debug",
    action="store_true",
    help="enable debug logging",
)
def cmd(debug: bool):
    log_level = logging.DEBUG if debug else logging.INFO
    log_format = "%(asctime)s - %(levelname)s - %(funcName)s: %(message)s"
    logging.basicConfig(level=log_level, format=log_format)

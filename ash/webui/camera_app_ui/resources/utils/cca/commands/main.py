# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from cca import cli
from cca.commands import bundle
from cca.commands import check_color_tokens
from cca.commands import check_strings
from cca.commands import deploy
from cca.commands import dev
from cca.commands import lint
from cca.commands import test
from cca.commands import tsc
from cca.commands import upload


@cli.root(children=[
    bundle.cmd,
    check_color_tokens.cmd,
    check_strings.cmd,
    deploy.cmd,
    dev.cmd,
    lint.cmd,
    test.cmd,
    tsc.cmd,
    upload.cmd,
])
@cli.option(
    "--debug",
    action="store_true",
    help="enable debug logging",
)
def cmd(debug: bool):
    dir = os.path.dirname(os.path.realpath(__file__))
    cca_root = os.path.realpath(os.path.join(dir, "../../.."))
    assert os.path.basename(cca_root) == "resources"
    os.chdir(cca_root)

    log_level = logging.DEBUG if debug else logging.INFO
    log_format = "%(asctime)s - %(levelname)s - %(funcName)s: %(message)s"
    logging.basicConfig(level=log_level, format=log_format)

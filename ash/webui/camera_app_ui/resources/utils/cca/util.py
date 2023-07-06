# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import shlex
import subprocess
from typing import List


@functools.lru_cache(1)
def get_chromium_root() -> str:
    """Gets the root of chromium checkout, typically at ~/chromium/src."""
    path = os.path.realpath("../../../../")
    assert os.path.basename(path) == "src"
    return path


def shell_join(cmd: List[str]) -> str:
    return " ".join(shlex.quote(c) for c in cmd)


def run(args: List[str]):
    logging.debug(f"$ {shell_join(args)}")
    subprocess.check_call(args)


def check_output(args: List[str]) -> str:
    logging.debug(f"$ {shell_join(args)}")
    return subprocess.check_output(args, text=True)


def run_node(args: List[str]):
    root = get_chromium_root()
    node = os.path.join(root, "third_party/node/linux/node-linux-x64/bin/node")
    binary = os.path.join(root, "third_party/node/node_modules", args[0])
    run([node, binary] + args[1:])


def get_gen_dir(board: str) -> str:
    root_dir = get_chromium_root()
    return os.path.join(root_dir, f"out_{board}/Release/gen")

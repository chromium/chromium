# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import pathlib
import shlex
import subprocess
from typing import Optional

from cra import cli


@functools.cache
def get_cra_root() -> pathlib.Path:
    """Gets the `resources` folder of recorder app."""
    file_path = pathlib.Path(__file__).resolve()
    cra_root = (file_path / "../../..").resolve()
    assert cra_root.name == "resources"
    return cra_root


@functools.cache
def get_chromium_root() -> pathlib.Path:
    """Gets the root of chromium checkout, typically at ~/chromium/src."""
    path = (get_cra_root() / "../../../../").resolve()
    assert path.name == "src"
    return path


@functools.cache
def get_strings_dir() -> pathlib.Path:
    """Gets the folder of chromeos strings where recorder_strings.grd is at."""
    return get_chromium_root() / "chromeos"


@functools.cache
def _resolve_build_dir(build_dir: str) -> pathlib.Path:
    if "/" in build_dir:
        # This is either a relative or absolute path.
        resolved_build_dir = get_chromium_root() / pathlib.Path(build_dir)
    else:
        # Argument is a board name
        resolved_build_dir = get_chromium_root() / f"out_{build_dir}/Release"

    assert resolved_build_dir.is_dir(), (
        f"Failed to find the build output dir {build_dir}."
        " Please check and build Chrome at least once.")

    return resolved_build_dir


def build_dir_option(optional: bool = False):
    # TODO(pihsun): Make this always optional by guessing the build_dir from
    # mtime of the out_{board} directory when it's not explicitly given.
    return cli.option(
        "--build_dir" if optional else "build_dir",
        help="""
            Board name or chrome build directory.
            Can either be a board name (e.g. betty), a relative path to chrome
            source directory (e.g. out_betty/Release), or an absolute path to
            the build directory.
            The provided folder is used for finding MWC and lit, which is board
            independent. All other board dependent references will be stubbed.
        """,
        type=_resolve_build_dir,
        metavar="<board name|build directory>",
    )


def shell_join(cmd: list[str]) -> str:
    return " ".join(shlex.quote(c) for c in cmd)


def run(args: list[str], *, cwd: Optional[pathlib.Path] = None):
    logging.debug(f"$ {shell_join(args)}")
    subprocess.check_call(args, cwd=cwd)


def check_output(args: list[str]) -> str:
    logging.debug(f"$ {shell_join(args)}")
    return subprocess.check_output(args, text=True)


def run_node(args: list[str], *, cwd: Optional[pathlib.Path] = None):
    root = get_chromium_root()
    node = root / "third_party/node/linux/node-linux-x64/bin/node"
    binary = root / "third_party/node/node_modules" / args[0]
    run([str(node), str(binary)] + args[1:], cwd=cwd)


def to_camel_case(s: str) -> str:
    """Converts CAPITAL_CASE to camelCase."""
    start, *rest = s.lower().split('_')
    return start + ''.join(part.capitalize() for part in rest)

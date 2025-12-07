# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for retrieving information about a Chromium checkout."""

import functools
import logging
import pathlib
import shutil
import subprocess


@functools.cache
def check_btrfs(root_path) -> bool:
    """Checks if the given path is on a btrfs partition.

    Args:
        root_path: The path to check.

    Returns:
        True if the given path can be verified to be on a btrfs partition,
        otherwise False.
    """
    result = subprocess.run(
        ['stat', '-c', '%i', root_path],
        capture_output=True,
        check=True,
    )
    inode_number = int(result.stdout.strip())
    btrfs = inode_number == 256
    logging.debug('btrfs (%d)' if btrfs else 'Not in btrfs (%d)', inode_number)
    if not btrfs:
        logging.warning(
            'Warning: This is not running in a btrfs environment which will '
            'lead to a much slower runtime. Please see the README.md for '
            'btrfs setup instructions.')
    return btrfs


@functools.cache
def get_gclient_root() -> pathlib.Path:
    """Retrieves the gclient root for the current checkout.

    Returns:
        A Path containing the absolute path to the gclient root for the current
        checkout.
    """
    result = subprocess.run(
        ['gclient', 'root'],
        capture_output=True,
        text=True,
        check=True,
    )
    return pathlib.Path(result.stdout.strip())


@functools.cache
def get_depot_tools_path() -> pathlib.Path | None:
    """Finds the path to the depot_tools directory."""
    gclient_path = shutil.which('gclient')
    if not gclient_path:
        return None
    return pathlib.Path(gclient_path).resolve().parent

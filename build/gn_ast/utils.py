# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pathlib
import subprocess

# These paths should be relative to repository root.
_BAD_FILES = [
    # Malformed BUILD.gn file, remove this entry once it is fixed.
    "third_party/swiftshader/tests/VulkanUnitTests/BUILD.gn",
]


def is_bad_gn_file(filepath: str, root: pathlib.Path) -> bool:
    relpath = os.path.relpath(filepath, root)
    for bad_filepath in _BAD_FILES:
        if relpath == bad_filepath:
            logging.info(f'Skipping {relpath}: found in _BAD_FILES list.')
            return True
    if not os.access(filepath, os.R_OK | os.W_OK):
        logging.info(f'Skipping {relpath}: Cannot read and write to it.')
        return True
    return False


def is_git_ignored(root: pathlib.Path, filepath: str) -> bool:
    # The command git check-ignore exits with 0 if the path is ignored, 1 if it
    # is not ignored.
    exit_code = subprocess.run(['git', 'check-ignore', '-q', filepath],
                               cwd=root).returncode
    return exit_code == 0

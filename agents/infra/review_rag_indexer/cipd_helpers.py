# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper code for interacting with CIPD."""

import contextlib
import logging
import pathlib
import posixpath
import subprocess
import tempfile

CIPD_INDEX_BASE = posixpath.join('infra', 'review_rag')


@contextlib.contextmanager
def initialize_cipd_root():
    """Initializes a CIPD root in a temporary directory on demand.

    Yields:
        The path to the CIPD root directory.
    """
    with tempfile.TemporaryDirectory() as temp_dir:
        cipd_root = pathlib.Path(temp_dir)
        logging.debug('Using %s as the CIPD root', cipd_root)
        subprocess.run(
            ['cipd', 'init', '-force',
             str(cipd_root)],
            check=True,
            capture_output=True,
            text=True,
        )
        yield cipd_root


def install_package(package: str, version: str,
                    cipd_root: pathlib.Path) -> bool:
    """Install a package into the CIPD root.

    Args:
        package: The CIPD package path to install.
        version: The version of the package to install.
        cipd_root: The CIPD root directory to install into.

    Returns:
        True if the package was successfully installed, otherwise False.
    """
    try:
        logging.debug('Installing CIPD package %s@%s', package, version)
        # This will install everything directly in the CIPD root, which should
        # be fine for our uses but does technically run the risk of conflicts
        # if multiple installed packages have identically named files or
        # directories.
        subprocess.run(
            [
                'cipd',
                'install',
                package,
                version,
                '-root',
                str(cipd_root),
                '-log-level',
                'warning',
            ],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        return True
    except subprocess.CalledProcessError as e:
        logging.warning(
            'Failed to install CIPD package %s@%s. This may be expected '
            'depending on the package. Output:\n%s', package, version,
            e.stdout)
        return False

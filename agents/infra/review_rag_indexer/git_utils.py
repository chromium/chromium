# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for interacting with the local git checkout."""

import collections.abc
import io
import logging
import subprocess


def read_files_at_revision(
        revision: str,
        paths: collections.abc.Iterable[str]) -> dict[str, str | None]:
    """Read one or more files at a given revision.

    Args;
        revision: The git revision to read the files at.
        paths: The filepaths to read.

    Returns:
        A dict mapping paths from `paths` to their content at `revision`. A
        value of None indicates that the file is either missing at `revision`
        or there was some error while attempting to read it.
    """
    if not paths:
        return {}
    input_lines = [f'{revision}:{path}' for path in paths]
    input_data = '\n'.join(input_lines) + '\n'
    result = subprocess.run(['git', 'cat-file', '--batch'],
                            input=input_data.encode('utf-8'),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            check=True,
                            text=False)
    stdout = result.stdout

    results = {}
    stdout_reader = io.BytesIO(stdout)
    for path in paths:
        header = stdout_reader.readline().decode('utf-8').strip()
        if not header:
            results[path] = None
            continue
        if 'missing' in header:
            results[path] = None
            continue
        parts = header.split()
        if len(parts) < 3:
            logging.error('Unexpected git cat-file header: %s', header)
            results[path] = None
            continue
        size = int(parts[2])
        content = stdout_reader.read(size)
        assert stdout_reader.read(1) == b'\n', stdout
        try:
            results[path] = content.decode('utf-8')
        except UnicodeDecodeError:
            logging.warning('Failed to decode content of %s at %s', path,
                            revision)
            results[path] = None
    return results


def revision_exists(revision: str) -> bool:
    """Check if a revision exists in the local git repository.

    Args:
        revision: The git revision to check.

    Returns:
        True if the given revision exists in the repo, otherwise False.
    """
    try:
        subprocess.run(['git', 'rev-parse', '--verify', revision],
                       stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL,
                       check=True)
        return True
    except (subprocess.CalledProcessError, OSError):
        return False

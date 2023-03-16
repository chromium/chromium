# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Exclusive filelocking for all supported platforms.

Copied from third_party/depot_tools/lockfile.py.
"""

import contextlib
import fcntl
import logging
import os
import time


class LockError(Exception):
    """Error raised if timeout or lock (without timeout) fails."""


def _open_file(lockfile):
    open_flags = (os.O_CREAT | os.O_WRONLY)
    return os.open(lockfile, open_flags, 0o644)


def _close_file(file_descriptor):
    os.close(file_descriptor)


def _lock_file(file_descriptor):
    fcntl.flock(file_descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)


def _try_lock(lockfile):
    f = _open_file(lockfile)
    try:
        _lock_file(f)
    except Exception:
        _close_file(f)
        raise
    return lambda: _close_file(f)


def _lock(path, timeout=0):
    """_lock returns function to release the lock if locking was successful.

    _lock also implements simple retry logic."""
    elapsed = 0
    while True:
        try:
            return _try_lock(path + '.locked')
        except (OSError, IOError) as error:
            if elapsed < timeout:
                sleep_time = min(10, timeout - elapsed)
                logging.info(
                    'Could not create lockfile; will retry after sleep(%d).',
                    sleep_time)
                elapsed += sleep_time
                time.sleep(sleep_time)
                continue
            raise LockError("Error locking %s (err: %s)" %
                            (path, str(error))) from error


@contextlib.contextmanager
def lock(path, timeout=0):
    """Get exclusive lock to path.

    Usage:
        import lockfile
        with lockfile.lock(path, timeout):
            # Do something
            pass

     """
    release_fn = _lock(path, timeout)
    try:
        yield
    finally:
        release_fn()

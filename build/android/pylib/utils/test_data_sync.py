# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Skips redundant gtest data pushes using a stamp stored on the device.

The stamp hashes (path, mtime, size) of every host file that would be pushed.
It is stored in a JSON marker file at the device test data root, which is shared
by all suites and checkouts, so a push is only skipped if the marker holds
exactly the current stamp.
"""

import hashlib
import json
import logging
import os
import posixpath

from devil.android import device_errors

logger = logging.getLogger(__name__)

# Bump when the stamp computation changes, to invalidate existing markers.
_STAMP_VERSION = 1
_DEVICE_ERRORS = (
    device_errors.CommandFailedError,
    device_errors.CommandTimeoutError,
)
_MARKER_NAME = '.chromium_test_data_stamp'


def InvalidateMarker(device, device_root, as_root=False):
    """Removes the marker. Call before any push of test data to device_root.

    Uncached pushes must call this too: they can modify data that another
    suite's marker vouches for without deleting the marker (PushChangedFiles()
    only deletes stale files in the directories it pushes to).
    """
    marker = posixpath.join(device_root, _MARKER_NAME)
    try:
        device.RemovePath(marker, force=True, as_root=as_root)
    except _DEVICE_ERRORS as e:
        logger.warning('Failed to remove %s: %s', marker, e)


def PreparePush(
    device, device_root, resolved_tuples, *, enabled, as_root=False
):
    """Checks the stamp marker before pushing test data to device_root.

    Returns None when enabled and the device already has the current data.
    Otherwise removes the marker (so an interrupted push leaves no valid marker
    behind) and returns a zero-arg commit function to call after pushing.
    """
    if not enabled:
        InvalidateMarker(device, device_root, as_root=as_root)
        return lambda: None

    marker = posixpath.join(device_root, _MARKER_NAME)
    # Computed before pushing, so that host files modified during the push
    # are not recorded as pushed.
    stamp = ComputeStamp(resolved_tuples)
    if _ReadMarkerStamp(device, marker, as_root) == stamp:
        logger.info('Test data in %s is up-to-date.', device_root)
        return None

    InvalidateMarker(device, device_root, as_root=as_root)

    def commit():
        content = json.dumps({'stamp': stamp})
        try:
            device.WriteFile(marker, content, as_root=as_root)
        except _DEVICE_ERRORS as e:
            logger.warning('Failed to write %s: %s', marker, e)

    return commit


def ComputeStamp(resolved_tuples):
    h = hashlib.sha256()

    def add(*parts):
        h.update(repr(parts).encode())

    add(_STAMP_VERSION)
    for host_path, device_path in sorted(resolved_tuples):
        add('tuple', host_path, device_path)
        for path in _IterHostFiles(host_path):
            try:
                st = os.stat(path)
            except OSError:
                continue  # Omitting the entry already changes the stamp.
            add(path, st.st_mtime_ns, st.st_size)
    return h.hexdigest()


def _ReadMarkerStamp(device, marker, as_root):
    try:
        # check_return=False: a missing marker is expected, and devil logs
        # CRITICAL stack dumps (and retries) for failing commands.
        content = device.RunShellCommand(
            ['cat', marker],
            check_return=False,
            raw_output=True,
            as_root=as_root,
        )
        return json.loads(content).get('stamp')
    except (ValueError, AttributeError, TypeError) + _DEVICE_ERRORS:
        return None


def _IterHostFiles(host_path):
    """Yields the files DeviceUtils.PushChangedFiles() pushes for host_path.

    Mirrors DeviceUtils._GetChangedFiles(), which skips files in any directory
    whose absolute path contains '/.' and doesn't follow symlinked directories.
    """
    path = os.path.realpath(host_path)
    if not os.path.isdir(path):
        yield path
        return
    for root, dirs, files in os.walk(path):
        if os.path.sep + '.' in root:
            dirs[:] = []
            continue
        dirs.sort()
        for f in sorted(files):
            yield os.path.join(root, f)

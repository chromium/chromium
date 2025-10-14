# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for supporting additional functionality related to tempfile."""

import contextlib
import os
import pathlib
import tempfile
from typing import Generator


@contextlib.contextmanager
def mkstemp_closed(suffix=None,
                   prefix=None,
                   directory=None) -> Generator[pathlib.Path, None, None]:
    """Yields a filepath to a closed temporary file on disk.

    When the context manager goes out of scope, the temporary file is removed.

    In practice, this is similar to using tempfile.NamedTemporaryFile() with
    delete_on_close=False and then immediately closing the file. However, this
    has a few benefits over NamedTemporaryFile():
      * The caller does not need to manually close the file
      * This works on Python versions before Python 3.12 when delete_on_close
        was added
      * A pathlib.Path is provided instead of a string

    This is intended for use cases where we need to know where a file is on
    disk, but do not intend to immediately write content to it, such as when
    passing it as an argument to a subprocess.

    Args:
        See tempfile.mkstemp().
    """
    file_handle, file_path = tempfile.mkstemp(suffix=suffix,
                                              prefix=prefix,
                                              dir=directory)
    try:
        os.close(file_handle)
        yield pathlib.Path(file_path)
    finally:
        os.remove(file_path)

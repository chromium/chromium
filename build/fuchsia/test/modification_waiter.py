# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" An AbstractContextManager to wait the modifications to finish during exit.
"""

import os
import time
from contextlib import AbstractContextManager


class ModificationWaiter(AbstractContextManager):
    """ Exits if there is no modifications for a certain time period, or the
    timeout has been reached. """

    def __init__(self, path: str) -> None:
        self._path = path
        # Waits at most 60 seconds.
        self._timeout = 60
        # Exits early if no modification happened during last 5 seconds.
        self._quiet_time = 5

    def __enter__(self) -> None:
        # Do nothing, the logic happens in __exit__
        return

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        # The default log.dir is /tmp and it's not a good idea to monitor it.
        if not self._path:
            return False
        # Always consider the last modification happening now to avoid an
        # unexpected early return.
        last_mod_time = time.time()
        start_time = last_mod_time
        while True:
            cur_time = time.time()
            if cur_time - start_time >= self._timeout:
                break
            cur_mod_time = os.path.getmtime(self._path)
            if cur_mod_time > last_mod_time:
                last_mod_time = cur_mod_time
            elif cur_time - last_mod_time >= self._quiet_time:
                break
            time.sleep(1)

        # Do not suppress exceptions.
        return False

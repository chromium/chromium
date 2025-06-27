# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A simple wrapper of running gsutil.py from the depot_tools; all the
functions in this module are running in a separated process. """

import os
import subprocess
import sys

from typing import List, Optional

from common import DIR_SRC_ROOT


def _find_gsutil() -> Optional[str]:
    """ Returns the location of the gsutil.py. """
    if not os.path.isfile(
            os.path.join(DIR_SRC_ROOT, 'build', 'find_depot_tools.py')):
        # No gsutil.py and find_depot_tools.py wrapper, will run gsutil
        # directly.
        return None

    sys.path.append(os.path.join(DIR_SRC_ROOT, 'build'))
    # Do not pollute the environment, callers should not use find_depot_tools
    # directly.
    # pylint: disable=import-error, import-outside-toplevel
    import find_depot_tools
    # pylint: enable=import-error, import-outside-toplevel
    sys.path.pop()
    return os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py')


GSUTIL_PATH = _find_gsutil()


def run_gsutil(args: List[str]) -> None:
    """ Runs gsutil with |args| and throws CalledProcessError if the process
    failed. """
    if not GSUTIL_PATH:
        # Try to run gsutil directly if there isn't a gsutil.py wrapper.
        return subprocess.run(['gsutil'] + args, check=True)
    return subprocess.run([sys.executable, GSUTIL_PATH] + args, check=True)

#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Location of the hermetic JDK. Prints JAVA_HOME when run as a script.

Resolved where the code runs: remote actions from a non-Linux host run on Linux
workers, where linux-amd64/ holds the Linux JDK and current/ the host's JDK.
"""

import os
import sys

_SRC_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
)
_JDK_ROOT = os.path.join(_SRC_ROOT, 'third_party', 'jdk')
_LINUX_JDK_DIR = os.path.join(_JDK_ROOT, 'linux-amd64')

if sys.platform == 'darwin':
    JAVA_HOME = os.path.join(_JDK_ROOT, 'current', 'Contents', 'Home')
elif os.path.isdir(_LINUX_JDK_DIR):
    JAVA_HOME = _LINUX_JDK_DIR
else:
    JAVA_HOME = os.path.join(_JDK_ROOT, 'current')

if __name__ == '__main__':
    print(JAVA_HOME)

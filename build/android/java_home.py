#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Location of the hermetic JDK. Prints JAVA_HOME when run as a script.

Resolved where the code runs: remote actions from a non-Linux host run on Linux
workers, where linux-amd64/ holds the Linux JDK and current/ the host's JDK.
"""

import os
import platform
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

# GraalVM native-images (e.g. jdk-tools-graalvm) exist only for linux-x64 hosts.
IS_LINUX_X64 = sys.platform == 'linux' and platform.machine() == 'x86_64'

# Tools bundled into bin/jdk-tools-graalvm on linux-x64.
_GRAALVM_TOOLS = frozenset(('javac', 'javap', 'jdeps'))


def ToolCmd(tool, java_home_dir=None):
    """Returns the command prefix for a JDK tool (javac, javap, jdeps)."""
    if java_home_dir is None:
        java_home_dir = JAVA_HOME
    if IS_LINUX_X64 and tool in _GRAALVM_TOOLS:
        return [os.path.join(java_home_dir, 'bin', 'jdk-tools-graalvm'), tool]
    return [os.path.join(java_home_dir, 'bin', tool)]


if __name__ == '__main__':
    print(JAVA_HOME)

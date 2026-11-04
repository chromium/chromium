#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script for java_deobfuscate.

This is also a buildable target, but having it pre-built here simplifies usage.
"""

import os
import platform
import sys

DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '../../../')
)
sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build', 'android'))
import java_home  # noqa: E402

_CHROME_R8_MAX_CPUS = 32


def main():
    chrome_r8_path = os.path.join(
        DIR_SOURCE_ROOT, 'third_party', 'r8', 'cipd', 'bin', 'chrome_r8'
    )
    if (
        sys.platform == 'linux'
        and platform.machine() == 'x86_64'
        and os.path.exists(chrome_r8_path)
    ):
        cmd = [chrome_r8_path]
        # Limit G1 GC threads on many-core machines (reduces sys time).
        if (os.cpu_count() or 0) > _CHROME_R8_MAX_CPUS:
            cmd += [f'-XX:ActiveProcessorCount={_CHROME_R8_MAX_CPUS}']
        cmd += ['--retrace']
    else:
        classpath = [
            os.path.join(
                DIR_SOURCE_ROOT,
                'build',
                'android',
                'stacktrace',
                'java_deobfuscate_java.jar',
            ),
            os.path.join(
                DIR_SOURCE_ROOT, 'third_party', 'r8', 'cipd', 'lib', 'r8.jar'
            ),
        ]
        java_path = os.path.join(java_home.JAVA_HOME, 'bin', 'java')

        cmd = [
            java_path,
            '-classpath',
            ':'.join(classpath),
            'org.chromium.build.FlushingReTrace',
        ]
    cmd.extend(sys.argv[1:])

    os.execvp(cmd[0], cmd)


if __name__ == '__main__':
    main()

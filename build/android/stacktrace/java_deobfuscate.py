#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper script for java_deobfuscate.

This is also a buildable target, but having it pre-built here simplifies usage.
"""

import os
import sys

DIR_SOURCE_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '../../../'))

def main():
  classpath = [
      os.path.join(DIR_SOURCE_ROOT, 'build', 'android', 'stacktrace',
                   'java_deobfuscate_java.jar'),
      os.path.join(DIR_SOURCE_ROOT, 'third_party', 'r8', 'cipd', 'lib',
                   'r8.jar')
  ]
  java_path = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'jdk', 'current',
                           'bin', 'java')

  cmd = [
      java_path, '-classpath', ':'.join(classpath),
      'org.chromium.build.FlushingReTrace'
  ]
  cmd.extend(sys.argv[1:])

  os.execvp(cmd[0], cmd)


if __name__ == '__main__':
  main()

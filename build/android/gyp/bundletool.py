#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple wrapper around the bundletool tool.

Bundletool is distributed as a versioned jar file. This script abstracts the
location and version of this jar file, as well as the JVM invokation."""

import logging
import os
import sys

from util import build_utils

# Assume this is stored under build/android/gyp/
BUNDLETOOL_DIR = os.path.abspath(os.path.join(
    __file__, '..', '..', '..', '..', 'third_party', 'android_build_tools',
    'bundletool'))

BUNDLETOOL_VERSION = '1.4.0'

BUNDLETOOL_JAR_PATH = os.path.join(
    BUNDLETOOL_DIR, 'bundletool-all-%s.jar' % BUNDLETOOL_VERSION)


def RunBundleTool(args, warnings_as_errors=(), print_stdout=False):
  # Use () instead of None because command-line flags are None by default.
  verify = warnings_as_errors == () or warnings_as_errors
  # ASAN builds failed with the default of 1GB (crbug.com/1120202).
  # Bug for bundletool: https://issuetracker.google.com/issues/165911616
  cmd = build_utils.JavaCmd(verify, xmx='4G')
  cmd += ['-jar', BUNDLETOOL_JAR_PATH]
  cmd += args
  logging.debug(' '.join(cmd))
  return build_utils.CheckOutput(
      cmd,
      print_stdout=print_stdout,
      print_stderr=True,
      fail_on_output=False,
      stderr_filter=build_utils.FilterReflectiveAccessJavaWarnings)


if __name__ == '__main__':
  RunBundleTool(sys.argv[1:], print_stdout=True)

#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple wrapper around the bundletool tool.

Bundletool is distributed as a versioned jar file. This script abstracts the
location and version of this jar file, as well as the JVM invokation."""

import logging
import os
import subprocess
import sys

from util import build_utils

# Assume this is stored under build/android/gyp/
BUNDLETOOL_DIR = os.path.abspath(os.path.join(
    __file__, '..', '..', '..', '..', 'third_party', 'android_build_tools',
    'bundletool'))

BUNDLETOOL_VERSION = '0.10.3'

BUNDLETOOL_JAR_PATH = os.path.join(
    BUNDLETOOL_DIR, 'bundletool-all-%s.jar' % BUNDLETOOL_VERSION)

def RunBundleTool(args):
  args = [build_utils.JAVA_PATH, '-jar', BUNDLETOOL_JAR_PATH] + args
  logging.debug(' '.join(args))
  subprocess.check_call(args)

if __name__ == '__main__':
  RunBundleTool(sys.argv[1:])

#!/usr/bin/env vpython

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Runs component smoketests for WebView """

import argparse
import os
import sys

SRC_DIR = os.path.join(os.path.dirname(__file__),
                       os.pardir,
                       os.pardir,
                       os.pardir)
PERF_DIR = os.path.join(SRC_DIR, 'tools', 'perf')
PY_UTILS_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'common', 'py_utils')

if PERF_DIR not in sys.path:
  sys.path.append(PERF_DIR)

if PY_UTILS_DIR not in sys.path:
  sys.path.append(PY_UTILS_DIR)

from chrome_telemetry_build import chromium_config
from core import path_util

path_util.AddTelemetryToPath()

from telemetry.testing import browser_test_runner

def main(args):
  config = chromium_config.ChromiumConfig(
      top_level_dir=os.path.dirname(__file__),
      benchmark_dirs=[os.path.dirname(__file__)])
  return browser_test_runner.Run(config, args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

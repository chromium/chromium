#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
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

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
PERF_DIR = os.path.join(SRC_DIR, 'tools', 'perf')
PY_UTILS_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'common', 'py_utils')

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

if PERF_DIR not in sys.path:
  sys.path.append(PERF_DIR)

if PY_UTILS_DIR not in sys.path:
  sys.path.append(PY_UTILS_DIR)

from chrome_telemetry_build import chromium_config
from core import path_util
from pylib import constants

path_util.AddTelemetryToPath()

from telemetry.testing import browser_test_runner

def main(args):
  parser = argparse.ArgumentParser(
      description='Extra argument parser', add_help=False)
  parser.add_argument('--output-directory', action='store', default=None,
                      help='Sets the CHROMIUM_OUTPUT_DIR environment variable')
  known_options, rest_args = parser.parse_known_args(args)

  constants.SetOutputDirectory(
      os.path.realpath(known_options.output_directory or os.getcwd()))

  config = chromium_config.ChromiumConfig(
      top_level_dir=os.path.dirname(__file__),
      benchmark_dirs=[os.path.dirname(__file__)])

  ret_val =  browser_test_runner.Run(config, rest_args)
  if '--help' in rest_args or '-h' in rest_args:
    print('\n\nCommand line arguments used in '
          'run_webview_component_smoketest.py')
    parser.print_help()

  return ret_val



if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

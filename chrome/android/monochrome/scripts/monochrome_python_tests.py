#!/usr/bin/env python2.7
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CUR_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(CUR_DIR))))
TYP_DIR = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if TYP_DIR not in sys.path:
  sys.path.insert(0, TYP_DIR)

import typ

def create_argument_parser():
  """ Creates command line parser. """
  parser = typ.ArgumentParser()
  required_args = parser.add_argument_group('required arguments')

  required_args.add_argument(
      '--monochrome-apk', required=True, help='The path to the monochrome APK.')
  parser.add_argument(
      '--monochrome-pathmap', help='The monochrome APK resources pathmap path.')
  required_args.add_argument(
      '--chrome-apk',
      required=True,
      help='The path to the chrome APK.')
  parser.add_argument(
      '--chrome-pathmap', help='The chrome APK resources pathmap path.')
  required_args.add_argument(
      '--system-webview-apk',
      required=True,
      help='The path to the system webview APK.')
  parser.add_argument(
      '--system-webview-pathmap',
      help='The system webview APK resources pathmap path.')

  # --avd-config parameter is unused. Add it to the parser because typ.Runner
  # checks that all arguments are known. crbug.com/1084351
  parser.add_argument('--avd-config', help='Unused')
  return parser


def main(argv):
  argument_parser = create_argument_parser()

  runner = typ.Runner()
  runner.parse_args(argument_parser, argv[1:])
  runner.args.top_level_dirs = [ os.path.dirname(__file__) ]
  runner.context = runner.args

  # Needs to be set to enable customizing runner.context
  runner.win_multiprocessing = typ.WinMultiprocessing.importable

  return_code, _, _ = runner.run()
  return return_code

if __name__ == '__main__':
  sys.exit(main(sys.argv))

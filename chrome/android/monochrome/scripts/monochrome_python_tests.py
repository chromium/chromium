#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_SRC_DIR = os.path.normpath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..'))
_TYP_DIR = os.path.join(
    _SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')
_DEVIL_CHROMIUM_DIR = os.path.join(_SRC_DIR, 'build', 'android')

sys.path[1:1] = [_TYP_DIR, _DEVIL_CHROMIUM_DIR]

import devil_chromium
import typ

# Import test files so they they are included in .pydeps.
import monochrome_dexdump_test
import monochrome_apk_checker_test


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

  # The following parameters are unused.
  # Add them to the parser because typ.Runner checks that all arguments
  # are known. crbug.com/1084351
  parser.add_argument('--avd-config', help='Unused')
  parser.add_argument('--emulator-debug-tags', help='Unused')
  parser.add_argument(
      '--use-persistent-shell',
      action='store_true',
      help='Unused')
  return parser


def main(argv):
  devil_chromium.Initialize()
  argument_parser = create_argument_parser()

  runner = typ.Runner()
  runner.parse_args(argument_parser, argv[1:])
  if argument_parser.exit_status is not None:
    return argument_parser.exit_status
  runner.args.top_level_dirs = [ os.path.dirname(__file__) ]
  runner.context = runner.args

  # Needs to be set to enable customizing runner.context
  runner.win_multiprocessing = typ.WinMultiprocessing.importable

  return_code, _, _ = runner.run()
  return return_code

if __name__ == '__main__':
  sys.exit(main(sys.argv))

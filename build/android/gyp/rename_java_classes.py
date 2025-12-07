#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import tempfile

from util import build_utils

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

import zip_helpers


def _RenameJars(options):
  with tempfile.NamedTemporaryFile() as temp:
    cmd = build_utils.JavaCmd() + [
        '-cp',
        options.r8_path,
        'com.android.tools.r8.relocator.RelocatorCommandLine',
        '--input',
        options.input_jar,
        '--output',
        temp.name,
    ]
    for mapping_rule in options.mapping_rules:
      cmd += ['--map', mapping_rule]

    build_utils.CheckOutput(cmd)
    # use zip_helper.merge_zips to hermetize the zip because R8 changes the
    # times and permissions inside the output jar for some reason.
    zip_helpers.merge_zips(options.output_jar, [temp.name])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-jar',
                      required=True,
                      help='Output file for the renamed classes jar')
  parser.add_argument('--input-jar',
                      required=True,
                      help='Input jar file to rename classes in')
  parser.add_argument('--r8-path', required=True, help='Path to R8 Jar')
  parser.add_argument('--map',
                      action='append',
                      dest='mapping_rules',
                      help='List of mapping rules in the form of ' +
                      '"<original prefix>.**-><new prefix>"')
  options = parser.parse_args()

  _RenameJars(options)


if __name__ == '__main__':
  main()

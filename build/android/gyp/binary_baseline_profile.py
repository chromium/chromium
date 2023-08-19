#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a binary profile from an HRF + dex + mapping."""

import argparse
import sys

from util import build_utils
import action_helpers


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--output-profile',
                      required=True,
                      help='Path to output binary profile.')
  parser.add_argument('--output-metadata',
                      required=True,
                      help='Path to output binary profile metadata.')
  parser.add_argument('--profgen',
                      required=True,
                      help='Path to profgen binary.')
  parser.add_argument('--dex',
                      required=True,
                      help='Path to a zip containing release dex files.')
  parser.add_argument('--proguard-mapping',
                      help='Path to proguard mapping for release dex.')
  parser.add_argument('--input-profile-path',
                      required=True,
                      help='Path to HRF baseline profile to apply.')
  options = parser.parse_args(build_utils.ExpandFileArgs(args))

  cmd = [
      options.profgen,
      'bin',
      options.input_profile_path,
      '-o',
      options.output_profile,
      '-om',
      options.output_metadata,
      '-a',
      options.dex,
  ]

  if options.proguard_mapping:
    cmd += [
        '-m',
        options.proguard_mapping,
    ]

  build_utils.CheckOutput(cmd, env={'JAVA_HOME': build_utils.JAVA_HOME})
  action_helpers.write_depfile(options.depfile,
                               options.output_profile,
                               inputs=[options.dex])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

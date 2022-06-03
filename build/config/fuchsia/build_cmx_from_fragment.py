# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a complete CMX (v1) component manifest, from a program name and
   manifest fragment file."""

import argparse
import json
import sys


def BuildCmxFromFragment(output_file, fragment_file, program_binary):
  """Reads a CMX fragment specifying e.g. features & sandbox, and a program
     binary's filename, and writes out the full CMX.

     output_file: Build-relative filename at which to write the full CMX.
     fragment_file: Build-relative filename of the CMX fragment to read from.
     program_binary: Package-relative filename of the program binary.
  """

  with open(output_file, 'w') as component_manifest_file:
    component_manifest = json.load(open(fragment_file, 'r'))
    component_manifest.update({
        'program': {
            'binary': program_binary
        },
    })
    json.dump(component_manifest, component_manifest_file)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--cmx-fragment',
      required=True,
      help='Path to the CMX fragment to read from')
  parser.add_argument(
      '--cmx', required=True, help='Path to write the complete CMX file to')
  parser.add_argument(
      '--program',
      required=True,
      help='Package-relative path to the program binary')
  args = parser.parse_args()

  return BuildCmxFromFragment(args.cmx, args.cmx_fragment, args.program)


if __name__ == '__main__':
  sys.exit(main())

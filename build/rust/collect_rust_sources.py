#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Is used to find all rust files in a crate, and write the result to a
depfile. Then, used again to read the same depfile and pull out just the
source files. Lastly, it is also used to write a stamp file at the same
location as the depfile.'''

import argparse
import re
import subprocess
import sys

FILE_REGEX = re.compile('^(.*):')


def main():
  parser = argparse.ArgumentParser(
      description='Collect Rust sources for a crate')
  parser.add_argument('--stamp',
                      action='store_true',
                      help='Generate a stamp file')
  parser.add_argument('--generate-depfile',
                      action='store_true',
                      help='Generate a depfile')
  parser.add_argument('--read-depfile',
                      action='store_true',
                      help='Read the previously generated depfile')
  args, rest = parser.parse_known_args()

  if (args.stamp):
    stampfile = rest[0]
    with open(stampfile, "w") as f:
      f.write("stamp")
  elif (args.generate_depfile):
    rustc = rest[0]
    crate_root = rest[1]
    depfile = rest[2]
    rustflags = rest[3:]

    rustc_args = [
        "--emit=dep-info=" + depfile, "-Zdep-info-omit-d-target", crate_root
    ]
    subprocess.check_call([rustc] + rustc_args + rustflags)
  elif (args.read_depfile):
    depfile = rest[0]
    try:
      with open(depfile, "r") as f:
        files = [FILE_REGEX.match(l) for l in f.readlines()]
        for f in files:
          if f:
            print(f.group(1))
    except:
      pass
  else:
    print("ERROR: Unknown action")
    parser.print_help()
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import subprocess
import sys

# This script executes a command and redirects the stdout to a file. This is
# equivalent to |command... > output_file|.
#
# Usage: python redirect_stdout.py output_file command...

if __name__ == '__main__':
  if len(sys.argv) < 2:
    print("Usage: %s output_file command..." % sys.argv[0], file=sys.stderr)
    sys.exit(1)

  # This script is designed to run binaries produced by the current build. We
  # may prefix it with "./" to avoid picking up system versions that might
  # also be on the path.
  path = sys.argv[2]
  if not os.path.isabs(path):
    path = './' + path

  with open(sys.argv[1], 'w') as fp:
    sys.exit(subprocess.check_call([path] + sys.argv[3:], stdout=fp))

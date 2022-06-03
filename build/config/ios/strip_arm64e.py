# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Strip arm64e architecture from a binary if present."""

import argparse
import os
import shutil
import subprocess
import sys


def check_output(command):
  """Returns the output from |command| or propagates error, quitting script."""
  process = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  outs, errs = process.communicate()
  if process.returncode:
    sys.stderr.write('error: command failed with retcode %d: %s\n\n' %
                     (process.returncode, ' '.join(map(repr, command))))
    sys.stderr.write(errs.decode('UTF-8', errors='ignore'))
    sys.exit(process.returncode)
  return outs.decode('UTF-8')


def check_call(command):
  """Invokes |command| or propagates error."""
  check_output(command)


def parse_args(args):
  """Parses the command-line."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', required=True, help='Path to input binary')
  parser.add_argument('--output', required=True, help='Path to output binary')
  parser.add_argument('--xcode-version', required=True, help='Version of Xcode')
  return parser.parse_args(args)


def get_archs(path):
  """Extracts the architectures present in binary at |path|."""
  outputs = check_output(["xcrun", "lipo", "-info", os.path.abspath(path)])
  return outputs.split(': ')[-1].split()


def main(args):
  parsed = parse_args(args)

  outdir = os.path.dirname(parsed.output)
  if not os.path.isdir(outdir):
    os.makedirs(outdir)

  if os.path.exists(parsed.output):
    os.unlink(parsed.output)

  # As "lipo" fails with an error if asked to remove an architecture that is
  # not included, only use it if "arm64e" is present in the binary. Otherwise
  # simply copy the file.
  if 'arm64e' in get_archs(parsed.input):
    check_output([
        "xcrun", "lipo", "-remove", "arm64e", "-output",
        os.path.abspath(parsed.output),
        os.path.abspath(parsed.input)
    ])
  else:
    shutil.copy(parsed.input, parsed.output)


if __name__ == '__main__':
  main(sys.argv[1:])

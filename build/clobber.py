#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script provides methods for clobbering build directories."""

import argparse
import os
import subprocess
import sys


_SRC_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _gn_cmd(cmd, build_dir):
  gn_exe = 'gn'
  if sys.platform == 'win32':
    gn_exe += '.bat'
  return [gn_exe, cmd, '--root=%s' % _SRC_DIR, '-C', build_dir]


def _generate_build_ninja(build_dir):
  gn_gen_cmd = _gn_cmd('gen', build_dir)
  print('Running %s' % ' '.join(gn_gen_cmd))
  subprocess.run(gn_gen_cmd, check=True)


def _clean_build_dir(build_dir):
  print('Cleaning %s' % build_dir)

  gn_clean_cmd = _gn_cmd('clean', build_dir)
  print('Running %s' % ' '.join(gn_clean_cmd))
  try:
    subprocess.run(gn_clean_cmd, check=True)
  except Exception:
    # gn clean may fail when build.ninja is corrupted or missing.
    # Regenerate build.ninja and retry gn clean again.
    _generate_build_ninja(build_dir)
    print('Running %s' % ' '.join(gn_clean_cmd))
    subprocess.run(gn_clean_cmd, check=True)


def clobber(out_dir):
  """Clobber contents of build directory."""
  for f in os.listdir(out_dir):
    path = os.path.join(out_dir, f)
    if os.path.isdir(path):
      _clean_build_dir(path)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', help='The output directory to clobber')
  args = parser.parse_args()
  clobber(args.out_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())

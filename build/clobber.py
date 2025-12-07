#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script provides methods for clobbering build directories."""

import argparse
import os
import shutil
import subprocess
import sys


def extract_gn_build_commands(build_ninja_file):
  """Extracts from a build.ninja the commands to run GN.

  The commands to run GN are the gn rule and build.ninja build step at the
  top of the build.ninja file. We want to keep these when deleting GN builds
  since we want to preserve the command-line flags to GN.

  On error, returns the empty string."""
  result = ""
  with open(build_ninja_file, 'r') as f:
    # Reads until the first empty line after the "build build.ninja:" target.
    # We assume everything before it necessary as well (eg the
    # "ninja_required_version" line).
    found_build_dot_ninja_target = False
    for line in f.readlines():
      result += line
      if line.startswith('build build.ninja:'):
        found_build_dot_ninja_target = True
      if found_build_dot_ninja_target and line[0] == '\n':
        return result
  return ''  # We got to EOF and didn't find what we were looking for.


def _rmtree(d):
  # For unknown reasons (anti-virus?) rmtree of Chromium build directories
  # often fails on Windows.
  if sys.platform.startswith('win'):
    subprocess.check_call(['rmdir', '/s', '/q', d], shell=True)
  else:
    shutil.rmtree(d)


def _clean_dir(build_dir):
  # Remove files/sub directories individually instead of recreating the build
  # dir because it fails when the build dir is symlinked or mounted.
  for e in os.scandir(build_dir):
    if e.is_dir():
      _rmtree(e.path)
    else:
      os.remove(e.path)


def delete_build_dir(build_dir):
  # GN writes a build.ninja.d file. Note that not all GN builds have args.gn.
  build_ninja_d_file = os.path.join(build_dir, 'build.ninja.d')
  if not os.path.exists(build_ninja_d_file):
    _clean_dir(build_dir)
    return

  # GN builds aren't automatically regenerated when you sync. To avoid
  # messing with the GN workflow, erase everything but the args file, and
  # write a dummy build.ninja file that will automatically rerun GN the next
  # time Ninja is run.
  build_ninja_file = os.path.join(build_dir, 'build.ninja')
  build_commands = extract_gn_build_commands(build_ninja_file)

  try:
    gn_args_file = os.path.join(build_dir, 'args.gn')
    with open(gn_args_file, 'r') as f:
      args_contents = f.read()
  except IOError:
    args_contents = ''

  exception_during_rm = None
  try:
    # _clean_dir() may fail, such as when chrome.exe is running,
    # and we still want to restore args.gn/build.ninja/build.ninja.d, so catch
    # the exception and rethrow it later.
    # We manually rm files inside the build dir rather than using "gn clean/gen"
    # since we may not have run all necessary DEPS hooks yet at this point.
    _clean_dir(build_dir)
  except Exception as e:
    exception_during_rm = e

  # Put back the args file (if any).
  if args_contents != '':
    with open(gn_args_file, 'w') as f:
      f.write(args_contents)

  # Write the build.ninja file sufficiently to regenerate itself.
  with open(os.path.join(build_dir, 'build.ninja'), 'w') as f:
    if build_commands != '':
      f.write(build_commands)
    else:
      # Couldn't parse the build.ninja file, write a default thing.
      f.write('''ninja_required_version = 1.7.2

rule gn
  command = gn -q gen //out/%s/
  description = Regenerating ninja files

build build.ninja: gn
  generator = 1
  depfile = build.ninja.d
''' % (os.path.split(build_dir)[1]))

  # Write a .d file for the build which references a nonexistant file. This
  # will make Ninja always mark the build as dirty.
  with open(build_ninja_d_file, 'w') as f:
    f.write('build.ninja: nonexistant_file.gn\n')

  if exception_during_rm:
    # Rethrow the exception we caught earlier.
    raise exception_during_rm


def clobber(out_dir):
  """Clobber contents of build sub directories.

  Don't delete the directory itself: some checkouts have the build directory
  mounted."""
  for f in os.listdir(out_dir):
    path = os.path.join(out_dir, f)
    if os.path.isdir(path):
      delete_build_dir(path)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('out_dir', help='The output directory to clobber')
  args = parser.parse_args()
  clobber(args.out_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recursively create hardlinks to targets at output."""


import argparse
import os
import shutil
import sys


def CreateHardlinkHelper(target, output):
  """
  Creates hardlink to `target` at `output`.

  If `target` is a directory, the directory structure will be copied and
  each file will be hardlinked independently. If `target` is a symlink,
  a new symlink will be created.

  The parent directory of `output` must exists or the function will fail.
  """
  if os.path.islink(target):
    os.symlink(os.readlink(target), output)
  elif os.path.isfile(target):
    try:
      os.link(target, output)
    except:
      shutil.copy(target, output)
  else:
    os.mkdir(output)
    for name in os.listdir(target):
      CreateHardlinkHelper(
          os.path.join(target, name),
          os.path.join(output, name))


def CreateHardlink(target, output):
  """
  Creates hardlink to `target` at `output`.

  If `target` is a directory, the directory structure will be copied and
  each file will be hardlinked independently. If `target` is a symlink,
  a new symlink will be created.

  If `output` already exists, it is first deleted. The parent directory
  of `output` is created if it does not exists.
  """
  if os.path.exists(output):
    if os.path.isdir(output):
      shutil.rmtree(output)
    else:
      os.unlink(output)
  dirname = os.path.dirname(output)
  if not os.path.isdir(dirname):
    os.makedirs(dirname)
  CreateHardlinkHelper(target, output)


def CreateHardlinks(output_dir, relative_to, targets):
  """
  Creates hardlinks to `targets` in `output_dir`.

  The `targets` should starts with `relative_to` and the hardlink will
  be created at `{output_dir}/{os.path.relpath(sources, relative_to)}`.

  Fails with an error if any file in `targets` not located inside the
  `relative_to` directory or if creating any of the hardlinks fails.
  """
  for target in targets:
    if not target.startswith(relative_to):
      print(f'error: "{target}" not relative to "{relative_to}',
            file=sys.stderr)
      sys.exit(1)

  for target in targets:
    output = os.path.join(output_dir, os.path.relpath(target, relative_to))
    CreateHardlink(target, output)


def main(args):
  parser = argparse.ArgumentParser()

  parser.add_argument('--output-dir',
                      required=True,
                      help='directory where the hardlinks should be created')

  parser.add_argument('--relative-to',
                      required=True,
                      help='sources file will be rebased to this directory')

  parser.add_argument(
      'sources',
      nargs='+',
      help='files that should be hardlinked, must be below RELATIVE_TO')

  parsed = parser.parse_args(args)
  CreateHardlinks(os.path.normpath(parsed.output_dir),
                  os.path.normpath(parsed.relative_to) + os.sep,
                  [os.path.normpath(source) for source in parsed.sources])


if __name__ == '__main__':
  main(sys.argv[1:])

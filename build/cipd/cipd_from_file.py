#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate yaml file based on FILES.cfg."""

import argparse
import os


def _ParseFilesCfg(files_file):
  """Return the dictionary of archive file info read from the given file."""
  if not os.path.exists(files_file):
    raise IOError('Files list does not exist (%s).' % files_file)
  exec_globals = {'__builtins__': None}

  exec(open(files_file).read(), exec_globals)
  return exec_globals['FILES']


def _Process(args):
  yaml_content = ('package: ' + args.package + '\ndescription: ' +
                  args.description + '\ninstall_mode: ' + args.install_mode +
                  '\ndata:\n')
  fileobj = _ParseFilesCfg(args.files_file)
  for item in fileobj:
    if 'buildtype' in item:
      if args.buildtype not in item['buildtype']:
        continue
    if 'arch' in item:
      if args.arch not in item['arch']:
        continue
    if 'type' in item and item['type'] == 'folder':
      yaml_content += ' - dir: ' + item['filename'] + '\n'
    else:
      yaml_content += ' - file: ' + item['filename'] + '\n'

  with open(args.output_yaml_file, 'w') as f:
    f.write(yaml_content)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output_yaml_file', help='File to create.')
  parser.add_argument(
      '--package',
      help='The path where the package will be located inside the CIPD\
           repository.')
  parser.add_argument(
      '--description',
      help='Sets the "description" field in CIPD package definition.')
  parser.add_argument('--install_mode',
                      help='String, should be either "symlink" or "copy".')
  parser.add_argument('--files_file',
                      help='FILES.cfg describes what files to include.')
  parser.add_argument('--buildtype', help='buildtype for FILES.cfg.')
  parser.add_argument('--arch', help='arch for FILES.cfg')

  args = parser.parse_args()

  _Process(args)


if __name__ == '__main__':
  main()

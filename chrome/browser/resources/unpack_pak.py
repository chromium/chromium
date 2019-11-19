#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys


_HERE_PATH = os.path.join(os.path.dirname(__file__))

# The name of a dummy file to be updated always after all other files have been
# written. This file is declared as the "output" for GN's purposes
_TIMESTAMP_FILENAME = os.path.join('unpack.stamp')


_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.insert(1, os.path.join(_SRC_PATH, 'tools', 'grit'))
from grit.format import data_pack


def ParseLine(line):
  return re.match('  {"([^"]+)", ([^},]+)', line)

def GetFileAndDirName(out_path, pak_dir, resource_path):
  dirname = os.path.dirname(resource_path)
  # When files are generated, |dirname| becomes
  # @out_folder@/<gen_path>/path_to_resource. To make the structure look as if
  # this file was not generated, remove @out_folder@ and <gen_path>.
  if ('@out_folder@' in dirname):
    dirname = os.path.relpath(dirname, os.path.join('@out_folder@', pak_dir))
  filename = os.path.basename(resource_path)
  dirname = os.path.join(out_path, dirname)
  return (filename, dirname)

def Unpack(pak_path, out_path):
  pak_dir = os.path.dirname(pak_path)
  pak_id = os.path.splitext(os.path.basename(pak_path))[0]

  data = data_pack.ReadDataPack(pak_path)

  # Associate numerical grit IDs to strings.
  # For example 120045 -> 'IDR_SETTINGS_ABOUT_PAGE_HTML'
  resource_ids = dict()
  resources_path = os.path.join(pak_dir, 'grit', pak_id + '.h')
  with open(resources_path) as resources_file:
    for line in resources_file:
      res = re.match('^#define (\S*).* (\d+)\)?$', line)
      if res:
        resource_ids[int(res.group(2))] = res.group(1)
  assert resource_ids

  # Associate numerical string IDs to files.
  resource_filenames = dict()
  resources_map_path = os.path.join(pak_dir, 'grit', pak_id + '_map.cc')
  with open(resources_map_path) as resources_map:
    for line in resources_map:
      res = ParseLine(line)
      if res:
        resource_filenames[res.group(2)] = res.group(1)
  assert resource_filenames

  # Extract packed files, while preserving directory structure.
  for (resource_id, text) in data.resources.iteritems():
    (filename, dirname) = GetFileAndDirName(
        out_path, pak_dir, resource_filenames[resource_ids[resource_id]])
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    with open(os.path.join(dirname, filename), 'w') as file:
      file.write(text)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pak_file')
  parser.add_argument('--out_folder')
  args = parser.parse_args()

  Unpack(args.pak_file, args.out_folder)

  timestamp_file_path = os.path.join(args.out_folder, _TIMESTAMP_FILENAME)
  with open(timestamp_file_path, 'a'):
    os.utime(timestamp_file_path, None)


if __name__ == '__main__':
  main()

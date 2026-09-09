#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to unpack component extension resources from a .grd file.
"""

import argparse
import os
import shutil
import sys
import xml.etree.ElementTree as ET


def parse_grd_includes(grd_path, visited_grd_files):
  visited_grd_files.append(grd_path)
  tree = ET.parse(grd_path)
  root = tree.getroot()
  grd_dir = os.path.dirname(grd_path)
  includes = []

  for elem in root.iter():
    if elem.tag == 'part':
      part_file = elem.attrib.get('file')
      if part_file:
        part_path = os.path.join(grd_dir, part_file)
        includes.extend(parse_grd_includes(part_path, visited_grd_files))
    elif elem.tag == 'include':
      file_attr = elem.attrib.get('file')
      resource_path = elem.attrib.get('resource_path')
      if file_attr and resource_path:
        includes.append((file_attr, resource_path))

  return includes


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--in-grd', required=True,
                      help='Path to the input .grd file')
  parser.add_argument('--out-folder', required=True,
                      help='Destination folder')
  parser.add_argument('--root-gen-dir', required=True,
                      help='Path to root_gen_dir relative to current dir')
  parser.add_argument('--root-src-dir', required=True,
                      help='Path to root_src_dir relative to current dir')
  parser.add_argument('--grd-resource-path-prefix', default='',
                      help='Resource path prefix to strip from destination')
  parser.add_argument('--depfile', required=True,
                      help='Path to Ninja depfile to write')
  parser.add_argument('--stamp', required=True,
                      help='Target output path for the depfile rule')
  args = parser.parse_args()

  os.makedirs(args.out_folder, exist_ok=True)

  prefix = args.grd_resource_path_prefix
  if prefix and not prefix.endswith('/'):
    prefix += '/'

  visited_grd_files = []
  dep_files = []
  current_files = set()

  for src_template, resource_path in parse_grd_includes(
      args.in_grd, visited_grd_files):
    src_path = (src_template
                .replace('${root_gen_dir}', args.root_gen_dir)
                .replace('${root_src_dir}', args.root_src_dir))
    dep_files.append(src_path)

    dst_rel_path = resource_path
    if prefix and dst_rel_path.startswith(prefix):
      dst_rel_path = dst_rel_path[len(prefix):]

    dst_path = os.path.join(args.out_folder, dst_rel_path)
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    shutil.copy2(src_path, dst_path)
    current_files.add(os.path.abspath(dst_path))

  # Remove any files in out_folder that are not part of the extension.
  for root, dirs, files in os.walk(args.out_folder, topdown=False):
    for f in files:
      full_path = os.path.abspath(os.path.join(root, f))
      if full_path not in current_files:
        os.remove(full_path)
    for d in dirs:
      dir_path = os.path.join(root, d)
      if not os.listdir(dir_path):
        os.rmdir(dir_path)

  if not os.path.exists(args.stamp):
    raise RuntimeError(f'Expected stamp output {args.stamp} was not unpacked')
  all_deps = list(dict.fromkeys(visited_grd_files + dep_files))
  escaped_deps = [p.replace(' ', '\\ ') for p in all_deps]
  os.makedirs(os.path.dirname(args.depfile), exist_ok=True)
  with open(args.depfile, 'w') as f:
    f.write(f'{args.stamp}: {" ".join(escaped_deps)}\n')

  return 0


if __name__ == '__main__':
  sys.exit(main())

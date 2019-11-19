#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a compressed archive of unstripped binaries cataloged by
"ids.txt"."""

import argparse
import os
import subprocess
import sys
import tarfile


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('ids_txt', type=str, nargs=1,
                      help='Path to ids.txt files.')
  parser.add_argument('-o', '--output_tarball', nargs=1, type=str,
                      help='Path which the tarball will be written to.')
  parser.add_argument('--fuchsia-build-id-dir', type=str, required=True,
                      help='Directory containing symbols for SDK prebuilts.')
  args = parser.parse_args(args)

  ids_txt = args.ids_txt[0]
  build_ids_archive = tarfile.open(args.output_tarball[0], 'w:bz2')
  for line in open(ids_txt, 'r'):
    build_id, binary_path = line.strip().split(' ')

    # Look for prebuilt symbols in the SDK first.
    symbol_source_path = os.path.join(args.fuchsia_build_id_dir,
                                      build_id[:2],
                                      build_id[2:] + '.debug')
    if not os.path.exists(symbol_source_path):
      symbol_source_path = os.path.abspath(
          os.path.join(os.path.dirname(ids_txt), binary_path))

      if os.path.getsize(symbol_source_path) == 0:
        # This is a prebuilt which wasn't accompanied by SDK symbols.
        continue

    # Exclude stripped binaries (indicated by their lack of symbol tables).
    readelf_output = subprocess.check_output(
        ['readelf', '-S', symbol_source_path])
    if not '.symtab' in readelf_output:
      continue

    # Archive the unstripped ELF binary, placing it in a hierarchy keyed to the
    # GNU build ID. The binary resides in a directory whose name is the first
    # two characters of the build ID, with the binary file itself named after
    # the remaining characters of the build ID. So, a binary file with the build
    # ID "deadbeef" would be located at the path 'de/adbeef.debug'.
    build_ids_archive.add(symbol_source_path,
                          '%s/%s.debug' % (build_id[:2], build_id[2:]))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

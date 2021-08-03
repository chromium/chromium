#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts an LLD partition from an ELF file."""

import argparse
import hashlib
import math
import os
import struct
import subprocess
import sys
import tempfile


def _ComputeNewBuildId(old_build_id, file_path):
  """
    Computes the new build-id from old build-id and file_path.

    Args:
      old_build_id: Original build-id in bytearray.
      file_path: Path to output ELF file.

    Returns:
      New build id with the same length as |old_build_id|.
    """
  m = hashlib.sha256()
  m.update(old_build_id)
  m.update(os.path.basename(file_path).encode('utf-8'))
  hash_bytes = m.digest()
  # In case build_id is longer than hash computed, repeat the hash
  # to the desired length first.
  id_size = len(old_build_id)
  hash_size = len(hash_bytes)
  return (hash_bytes * (id_size // hash_size + 1))[:id_size]


def _ExtractPartition(objcopy, input_elf, output_elf, partition):
  """
  Extracts a partition from an ELF file.

  For partitions other than main partition, we need to rewrite
  the .note.gnu.build-id section so that the build-id remains
  unique.

  Note:
  - `objcopy` does not modify build-id when partitioning the
    combined ELF file by default.
  - The new build-id is calculated as hash of original build-id
    and partitioned ELF file name.

  Args:
    objcopy: Path to objcopy binary.
    input_elf: Path to input ELF file.
    output_elf: Path to output ELF file.
    partition: Partition to extract from combined ELF file. None when
      extracting main partition.
  """
  if not partition:  # main partition
    # We do not overwrite build-id on main partition to allow the expected
    # partition build ids to be synthesized given a libchrome.so binary,
    # if necessary.
    subprocess.check_call(
        [objcopy, '--extract-main-partition', input_elf, output_elf])
    return

  # partitioned libs
  build_id_section = '.note.gnu.build-id'

  with tempfile.TemporaryDirectory() as tempdir:
    temp_elf = os.path.join(tempdir, 'obj_without_id.so')
    old_build_id_file = os.path.join(tempdir, 'old_build_id')
    new_build_id_file = os.path.join(tempdir, 'new_build_id')

    # Dump out build-id section and remove original build-id section from
    # ELF file.
    subprocess.check_call([
        objcopy,
        '--extract-partition',
        partition,
        # Note: Not using '--update-section' here as it is not supported
        # by llvm-objcopy.
        '--remove-section',
        build_id_section,
        '--dump-section',
        '{}={}'.format(build_id_section, old_build_id_file),
        input_elf,
        temp_elf,
    ])

    with open(old_build_id_file, 'rb') as f:
      note_content = f.read()

    # .note section has following format according to <elf/external.h>
    #   typedef struct {
    #       unsigned char   namesz[4];  /* Size of entry's owner string */
    #       unsigned char   descsz[4];  /* Size of the note descriptor */
    #       unsigned char   type[4];    /* Interpretation of the descriptor */
    #       char        name[1];        /* Start of the name+desc data */
    #   } Elf_External_Note;
    # `build-id` rewrite is only required on Android platform,
    # where we have partitioned lib.
    # Android platform uses little-endian.
    # <: little-endian
    # 4x: Skip 4 bytes
    # L: unsigned long, 4 bytes
    descsz, = struct.Struct('<4xL').unpack_from(note_content)
    prefix = note_content[:-descsz]
    build_id = note_content[-descsz:]

    with open(new_build_id_file, 'wb') as f:
      f.write(prefix + _ComputeNewBuildId(build_id, output_elf))

    # Write back the new build-id section.
    subprocess.check_call([
        objcopy,
        '--add-section',
        '{}={}'.format(build_id_section, new_build_id_file),
        # Add alloc section flag, or else the section will be removed by
        # objcopy --strip-all when generating unstripped lib file.
        '--set-section-flags',
        '{}={}'.format(build_id_section, 'alloc'),
        temp_elf,
        output_elf,
    ])


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--partition',
      help='Name of partition if not the main partition',
      metavar='PART')
  parser.add_argument(
      '--objcopy',
      required=True,
      help='Path to llvm-objcopy binary',
      metavar='FILE')
  parser.add_argument(
      '--unstripped-output',
      required=True,
      help='Unstripped output file',
      metavar='FILE')
  parser.add_argument(
      '--stripped-output',
      required=True,
      help='Stripped output file',
      metavar='FILE')
  parser.add_argument('--dwp', help='Path to dwp binary', metavar='FILE')
  parser.add_argument('input', help='Input file')
  args = parser.parse_args()

  _ExtractPartition(args.objcopy, args.input, args.unstripped_output,
                    args.partition)
  subprocess.check_call([
      args.objcopy,
      '--strip-all',
      args.unstripped_output,
      args.stripped_output,
  ])

  if args.dwp:
    dwp_args = [
        args.dwp, '-e', args.unstripped_output, '-o',
        args.unstripped_output + '.dwp'
    ]
    # Suppress output here because it doesn't seem to be useful. The most
    # common error is a segfault, which will happen if files are missing.
    subprocess.check_output(dwp_args, stderr=subprocess.STDOUT)


if __name__ == '__main__':
  sys.exit(main())

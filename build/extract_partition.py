#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts an LLD partition from an ELF file."""

import argparse
import hashlib
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

    # Dump out build-id section.
    subprocess.check_call([
        objcopy,
        '--extract-partition',
        partition,
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

    # Update the build-id section.
    subprocess.check_call([
        objcopy,
        '--update-section',
        '{}={}'.format(build_id_section, new_build_id_file),
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
  parser.add_argument('--split-dwarf', action='store_true')
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

  # Debug info for partitions is the same as for the main library, so just
  # symlink the .dwp files.
  if args.split_dwarf:
    dest = args.unstripped_output + '.dwp'
    try:
      os.unlink(dest)
    except OSError:
      pass
    relpath = os.path.relpath(args.input + '.dwp', os.path.dirname(dest))
    os.symlink(relpath, dest)


if __name__ == '__main__':
  sys.exit(main())

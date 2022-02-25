#!/usr/bin/env python3
# coding: utf-8

# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Increases the size of a Mach-O image by adding padding to its __LINKEDIT
# segment.

import argparse
import os
import struct
import sys

# Constants from <mach-o/loader.h>.
_MH_MAGIC = 0xfeedface
_MH_MAGIC_64 = 0xfeedfacf
_LC_SEGMENT = 0x1
_LC_SEGMENT_64 = 0x19
_LC_CODE_SIGNATURE = 0x1d
_SEG_LINKEDIT = b'__LINKEDIT'.ljust(16, b'\x00')


class PadLinkeditError(Exception):
    pass


def _struct_read_unpack(file, format_or_struct):
    """Reads bytes from |file|, unpacking them via the struct module. This
    function is provided for convenience: the number of bytes to read from
    |file| is determined based on the size of data that the struct unpack
    operation will consume.

    Args:
        file: The file object to read from.

        format_or_struct: A string suitable for struct.unpack’s |format|
            argument, or a struct.Struct object. This will be used to determine
            the number of bytes to read and to perform the unpack.

    Returns:
        A tuple of unpacked items.
    """

    if isinstance(format_or_struct, struct.Struct):
        struc = format_or_struct
        return struc.unpack(file.read(struc.size))

    format = format_or_struct
    return struct.unpack(format, file.read(struct.calcsize(format)))


def PadLinkedit(file, size):
    """Takes |file|, a single-architecture (thin) Mach-O image, and increases
    its size to |size| by adding padding (NUL bytes) to its __LINKEDIT segment.
    If |file| is not a thin Mach-O image, if its structure is unexpected, if it
    is already larger than |size|, or if it is already code-signed, raises
    PadLinkeditError.

    The image must already have a __LINKEDIT segment, the load command for the
    __LINKEDIT segment must be the last segment load command in the image, and
    the __LINKEDIT segment contents must be at the end of the file.

    Args:
        file: The file object to read from and modify.

        size: The desired size of the file.

    Returns:
        None

    Raises:
        PadLinkeditError if |file| is not suitable for the operation.
    """

    file.seek(0, os.SEEK_END)
    current_size = file.tell()
    file.seek(0, os.SEEK_SET)

    magic, = _struct_read_unpack(file, '<I')
    if magic == _MH_MAGIC_64:
        bits, endian = 64, '<'
    elif magic == _MH_MAGIC:
        bits, endian = 32, '<'
    elif magic == struct.unpack('>I', struct.pack('<I', _MH_MAGIC_64)):
        bits, endian = 64, '>'
    elif magic == struct.unpack('>I', struct.pack('<I', _MH_MAGIC)):
        bits, endian = 32, '>'
    else:
        raise PadLinkeditError('unrecognized magic', magic)

    if bits == 64:
        lc_segment = _LC_SEGMENT_64
        segment_command_struct = struct.Struct(endian + '16s4Q4I')
    else:
        lc_segment = _LC_SEGMENT
        segment_command_struct = struct.Struct(endian + '16s8I')

    grow = size - current_size
    if grow < 0:
        raise PadLinkeditError('file would need to shrink', grow)

    (cputype, cpusubtype, filetype, ncmds, sizeofcmds,
     flags) = _struct_read_unpack(file,
                                  endian + '6I' + ('4x' if bits == 64 else ''))

    load_command_struct = struct.Struct(endian + '2I')
    found_linkedit = False
    segment_max_offset = 0

    # Iterate through the load commands. It’s possible to consider |sizeofcmds|,
    # but since the file is being edited in-place, that would just be a sanity
    # check.
    for load_command_index in range(ncmds):
        cmd, cmdsize = _struct_read_unpack(file, load_command_struct)
        consumed = load_command_struct.size
        if cmd == lc_segment:
            if found_linkedit:
                raise PadLinkeditError('__LINKEDIT segment not last')

            (segname, vmaddr, vmsize, fileoff, filesize, maxprot, initprot,
             nsects, flags) = _struct_read_unpack(file, segment_command_struct)
            consumed += segment_command_struct.size

            if segname == _SEG_LINKEDIT:
                found_linkedit = True

                if fileoff < segment_max_offset:
                    raise PadLinkeditError('__LINKEDIT data not last')
                if fileoff + filesize != current_size:
                    raise PadLinkeditError('__LINKEDIT data not at EOF')

                vmsize += grow
                filesize += grow
                file.seek(-segment_command_struct.size, os.SEEK_CUR)
                file.write(
                    segment_command_struct.pack(segname, vmaddr, vmsize,
                                                fileoff, filesize, maxprot,
                                                initprot, nsects, flags))

            segment_max_offset = max(segment_max_offset, fileoff + filesize)
        elif cmd == _LC_CODE_SIGNATURE:
            raise PadLinkeditError(
                'modifying an already-signed image would render it unusable')

        # Aside from the above, load commands aren’t being interpreted, or even
        # read, so skip ahead to the next one.
        file.seek(cmdsize - consumed, os.SEEK_CUR)

    if not found_linkedit:
        raise PadLinkeditError('no __LINKEDIT')

    # Add the padding to the __LINKEDIT segment data.
    file.seek(grow, os.SEEK_END)
    file.truncate()


def _main(args):
    parser = argparse.ArgumentParser(
        description=
        'Increase the size of a Mach-O image by adding padding to its ' +
        '__LINKEDIT segment.')
    parser.add_argument('file', help='The Mach-O file to modify')
    parser.add_argument('size',
                        type=int,
                        help='The desired final size of the file, in bytes')
    parsed = parser.parse_args()

    with open(parsed.file, 'r+b') as file:
        PadLinkedit(file, parsed.size)


if __name__ == '__main__':
    sys.exit(_main(sys.argv[1:]))

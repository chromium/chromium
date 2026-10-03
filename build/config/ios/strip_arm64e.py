# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Strip arm64e architecture from a binary if present."""

import argparse
import os
import shutil
import struct
import subprocess
import sys

_FAT_MAGIC = 0xCAFEBABE
_FAT_CIGAM = 0xBEBAFECA
_FAT_MAGIC_64 = 0xCAFEBABF
_FAT_CIGAM_64 = 0xBFBAFECA
_MH_MAGIC = 0xFEEDFACE
_MH_MAGIC_64 = 0xFEEDFACF

_CPU_TYPE_X86 = 7
_CPU_TYPE_X86_64 = 0x01000007
_CPU_TYPE_ARM = 12
_CPU_TYPE_ARM64 = 0x0100000C
_CPU_SUBTYPE_MASK = 0x00FFFFFF
_CPU_SUBTYPE_ARM64E = 2


def check_output(command):
    """Returns the output from |command| or propagates error, quitting script."""
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    outs, errs = process.communicate()
    if process.returncode:
        sys.stderr.write(
            'error: command failed with retcode %d: %s\n\n'
            % (process.returncode, ' '.join(map(repr, command)))
        )
        sys.stderr.write(errs.decode('UTF-8', errors='ignore'))
        sys.exit(process.returncode)
    return outs.decode('UTF-8')


def check_call(command):
    """Invokes |command| or propagates error."""
    check_output(command)


def parse_args(args):
    """Parses the command-line."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True, help='Path to input binary')
    parser.add_argument('--output', required=True, help='Path to output binary')
    parser.add_argument(
        '--xcode-version', required=True, help='Version of Xcode'
    )
    return parser.parse_args(args)


def _cpu_to_arch(cputype, cpusubtype):
    subtype = cpusubtype & _CPU_SUBTYPE_MASK
    if cputype == _CPU_TYPE_ARM64:
        return 'arm64e' if subtype == _CPU_SUBTYPE_ARM64E else 'arm64'
    if cputype == _CPU_TYPE_X86_64:
        return 'x86_64'
    if cputype == _CPU_TYPE_ARM:
        return 'arm'
    if cputype == _CPU_TYPE_X86:
        return 'i386'
    return f'unknown_{cputype:#x}_{subtype:#x}'


def _parse_macho_archs(path):
    """Parses Mach-O or Fat header at |path| and returns architecture names."""
    with open(path, 'rb') as f:
        header = f.read(12)
        if len(header) >= 8:
            be_magic = struct.unpack('>I', header[:4])[0]
            le_magic = struct.unpack('<I', header[:4])[0]
            if be_magic in (
                _FAT_MAGIC,
                _FAT_CIGAM,
                _FAT_MAGIC_64,
                _FAT_CIGAM_64,
            ):
                endian = '>' if be_magic in (_FAT_MAGIC, _FAT_MAGIC_64) else '<'
                is_64 = be_magic in (_FAT_MAGIC_64, _FAT_CIGAM_64)
                nfat_arch = struct.unpack(f'{endian}I', header[4:8])[0]
                if 0 < nfat_arch <= 32:
                    entry_fmt = f'{endian}IIQQII' if is_64 else f'{endian}IIIII'
                    entry_size = struct.calcsize(entry_fmt)
                    f.seek(8)
                    entries = f.read(nfat_arch * entry_size)
                    if len(entries) == nfat_arch * entry_size:
                        archs = []
                        for i in range(nfat_arch):
                            chunk = entries[
                                i * entry_size : (i + 1) * entry_size
                            ]
                            fields = struct.unpack(entry_fmt, chunk)
                            archs.append(_cpu_to_arch(fields[0], fields[1]))
                        return archs
            if len(header) >= 12:
                if le_magic in (_MH_MAGIC, _MH_MAGIC_64):
                    _, cputype, cpusubtype = struct.unpack('<III', header[:12])
                    return [_cpu_to_arch(cputype, cpusubtype)]
                if be_magic in (_MH_MAGIC, _MH_MAGIC_64):
                    _, cputype, cpusubtype = struct.unpack('>III', header[:12])
                    return [_cpu_to_arch(cputype, cpusubtype)]
    raise RuntimeError(f'Unrecognized Mach-O header in {path}')


def _strip_arch_macho(input_path, output_path, arch_to_remove='arm64e'):
    """Removes |arch_to_remove| slice from a Mach-O Fat binary in Python."""
    with open(input_path, 'rb') as f:
        data = f.read()
    if len(data) < 8:
        shutil.copy(input_path, output_path)
        return
    be_magic = struct.unpack('>I', data[:4])[0]
    if be_magic not in (_FAT_MAGIC, _FAT_CIGAM, _FAT_MAGIC_64, _FAT_CIGAM_64):
        shutil.copy(input_path, output_path)
        return

    endian = '>' if be_magic in (_FAT_MAGIC, _FAT_MAGIC_64) else '<'
    is_64 = be_magic in (_FAT_MAGIC_64, _FAT_CIGAM_64)
    magic, nfat_arch = struct.unpack(f'{endian}II', data[:8])
    entry_fmt = f'{endian}IIQQII' if is_64 else f'{endian}IIIII'
    entry_size = struct.calcsize(entry_fmt)

    kept_entries = []
    for i in range(nfat_arch):
        start = 8 + i * entry_size
        fields = struct.unpack(entry_fmt, data[start : start + entry_size])
        if _cpu_to_arch(fields[0], fields[1]) != arch_to_remove:
            kept_entries.append(fields)

    if not kept_entries:
        shutil.copy(input_path, output_path)
        return

    cur_offset = 8 + len(kept_entries) * entry_size
    new_headers = [struct.pack(f'{endian}II', magic, len(kept_entries))]
    slices = []
    for fields in kept_entries:
        cputype, cpusubtype, old_offset, size, align = fields[:5]
        align_bytes = 1 << min(align, 20)
        new_offset = (cur_offset + align_bytes - 1) & ~(align_bytes - 1)
        pad_len = new_offset - cur_offset
        if is_64:
            new_headers.append(
                struct.pack(
                    entry_fmt,
                    cputype,
                    cpusubtype,
                    new_offset,
                    size,
                    align,
                    fields[5],
                )
            )
        else:
            new_headers.append(
                struct.pack(
                    entry_fmt, cputype, cpusubtype, new_offset, size, align
                )
            )
        slices.append((pad_len, data[old_offset : old_offset + size]))
        cur_offset = new_offset + size

    with open(output_path, 'wb') as out_f:
        for hdr in new_headers:
            out_f.write(hdr)
        for pad_len, slice_bytes in slices:
            if pad_len > 0:
                out_f.write(b'\x00' * pad_len)
            out_f.write(slice_bytes)
    shutil.copymode(input_path, output_path)


def get_archs(path):
    """Extracts the architectures present in binary at |path|."""
    if sys.platform != 'darwin':
        return _parse_macho_archs(path)
    outputs = check_output(['xcrun', 'lipo', '-info', os.path.abspath(path)])
    return outputs.split(': ')[-1].split()


def main(args):
    parsed = parse_args(args)

    outdir = os.path.dirname(parsed.output)
    if not os.path.isdir(outdir):
        os.makedirs(outdir)

    if os.path.exists(parsed.output):
        os.unlink(parsed.output)

    # As "lipo" fails with an error if asked to remove an architecture that is
    # not included, only use it if "arm64e" is present in the binary. Otherwise
    # simply copy the file.
    if 'arm64e' in get_archs(parsed.input):
        if sys.platform != 'darwin':
            _strip_arch_macho(parsed.input, parsed.output, 'arm64e')
        else:
            check_output(
                [
                    'xcrun',
                    'lipo',
                    '-remove',
                    'arm64e',
                    '-output',
                    os.path.abspath(parsed.output),
                    os.path.abspath(parsed.input),
                ]
            )
    else:
        shutil.copy(parsed.input, parsed.output)


if __name__ == '__main__':
    main(sys.argv[1:])

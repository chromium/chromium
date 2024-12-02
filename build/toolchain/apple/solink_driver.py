#!/usr/bin/env python3

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Thin wrapper of linker_driver.py for solink action."""

import argparse
import io
import os
import subprocess
import sys

from linker_driver import LinkerDriver, LINKER_DRIVER_ARG_PREFIX

SOLINK_DRIVER_OTOOL_ARG_PREFIX = LINKER_DRIVER_ARG_PREFIX + "otool,"
SOLINK_DRIVER_NM_ARG_PREFIX = LINKER_DRIVER_ARG_PREFIX + "nm,"

# https://source.chromium.org/chromium/chromium/src/+/a62b2ab2981feac86268718fdece2b2416aa9a41:build/toolchain/apple/toolchain.gni;l=467


def reexport(dylib, tocname, otool):
    """Returns true if need to reexport TOC for dylib."""
    if (not os.path.exists(dylib)) or \
       (not os.path.exists(tocname)):
        return True
    p = subprocess.run([otool, '-l', dylib], capture_output=True)
    if p.returncode != 0:
        return True
    return b'LC_REEXPORT_DYLIB' in p.stdout


def extractTOC(dylib, otool, nm):
    """Extracts TOC from dylib."""
    toc = io.BytesIO()
    out = subprocess.check_output([otool, '-l', dylib])
    lines = out.split(b'\n')
    for i, line in enumerate(lines):
        if b'LC_ID_DYLIB' in line:
            toc.write(line + b'\n')
            for j in range(5):
                toc.write(lines[i + 1 + j] + b'\n')

    out = subprocess.check_output([nm, '-gPp', dylib])
    lines = out.split(b'\n')
    for i, line in enumerate(lines):
        line = b' '.join(line.split(b' ')[0:2])
        if b'U$$' not in line:
            toc.write(line + b'\n')
    return toc.getvalue()


def main():
    args = []
    tocname = ""
    dylib = ""
    otool = ""
    nm = ""
    skip = False

    for i, arg in enumerate(sys.argv):
        if skip:
            skip = False
            continue
        if arg == "--tocname":
            tocname = sys.argv[i + 1]
            skip = True
            continue
        if arg == "-o":
            dylib = sys.argv[i + 1]
            skip = True
            args.append(arg)
            args.append(dylib)
            continue
        if arg.startswith(SOLINK_DRIVER_OTOOL_ARG_PREFIX):
            otool = arg[len(SOLINK_DRIVER_OTOOL_ARG_PREFIX):]
            continue
        if arg.startswith(SOLINK_DRIVER_NM_ARG_PREFIX):
            nm = arg[len(SOLINK_DRIVER_NM_ARG_PREFIX):]
            continue
        args.append(arg)

    need_reexport = reexport(dylib, tocname, otool)

    LinkerDriver(args).run()

    oldTOC = None
    newTOC = extractTOC(dylib, otool, nm)
    if os.path.exists(tocname):
        with open(tocname, 'rb') as f:
            oldTOC = f.read()
    if need_reexport or newTOC != oldTOC:
        with open(tocname, 'wb') as f:
            f.write(newTOC)


if __name__ == '__main__':
    main()
    sys.exit(0)

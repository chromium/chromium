#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Compares the structure / alignment of .grd files.

This takes as inputs two .grd files and checks that they contain the same nodes
and define the same resources with the same names.

Writes a stamp file if this is the case, or removes the stamp file and prints
an error message, otherwise.
"""

import argparse
import os
import xml.etree.ElementTree as ET

EXACT_TAGS = {'grit', 'outputs', 'output', 'emit', 'release', 'if'}
RESOURCE_TAGS = {'include', 'message'}
IGNORE = {'ph', 'includes', 'messages', 'ex'}


def compare_exact(a, b):
    if len(a.attrib) != len(b.attrib):
        return f'Attribute count mismatch: {len(a.attrib)} vs {len(b.attrib)}'
    for key in a.attrib:
        va = a.attrib[key]
        vb = b.attrib[key]
        if va != vb:
            return f'Attribute mismatch for \'{key}\': {va} vs {vb}'
    return 0


def compare_resource(a, b):
    na = a.attrib['name']
    nb = b.attrib['name']
    if na != nb:
        return f'Resource name mismatch: {na} vs {nb}'
    return 0


def compare(a, b):
    if a.tag != b.tag:
        return f'Tag name mismatch: {a.tag} vs {b.tag}'

    if a.tag in EXACT_TAGS:
        result = compare_exact(a, b)
        if result != 0:
            return result
    elif a.tag in RESOURCE_TAGS:
        result = compare_resource(a, b)
        if result != 0:
            return result
    else:
        if a.tag not in IGNORE:
            return f'Unexpected tag: {a.tag}'
    ca = list(a)
    cb = list(b)
    if len(ca) != len(cb):
        return f'Child count mismatch: {len(ca)} vs {len(cb)}'
    for i in range(len(ca)):
        result = compare(ca[i], cb[i])
        if result != 0:
            return result
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=__doc__.strip().splitlines()[0],
        epilog=' '.join(__doc__.strip().splitlines()[1:]))
    parser.add_argument('--stamp', help='The check file to output')
    parser.add_argument('lhs', help='The first .grd file')
    parser.add_argument('rhs', help='The second .grd file')
    args = parser.parse_args()
    a = ET.parse(args.lhs)
    b = ET.parse(args.rhs)
    result = compare(a.getroot(), b.getroot())
    # Always delete the stamp to create with a new timestamp.
    if os.path.exists(args.stamp):
        os.remove(args.stamp)
    if result != 0:
        print(result)
    else:
        with open(args.stamp, 'a') as _:
            pass


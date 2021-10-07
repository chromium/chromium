#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for create_unwind_table.py.

This test suite contains tests for the custom unwind table creation for 32-bit
arm builds.
"""

import ctypes
import io
import unittest

from create_unwind_table import AddressCfi, FilterToNonTombstoneCfi, \
 FunctionCfi, ReadFunctionCfi


class _TestReadFunctionCfi(unittest.TestCase):
  def testFilterTombstone(self):
    input_lines = [
        'file name',
        'STACK CFI INIT 0 ',
        'STACK CFI 100 ',
        'STACK CFI INIT 1 ',
        'STACK CFI 200 ',
    ]

    f = io.StringIO(''.join(line + '\n' for line in input_lines))

    self.assertListEqual([
        'STACK CFI INIT 1 \n',
        'STACK CFI 200 \n',
    ], list(FilterToNonTombstoneCfi(f)))

  def testReadFunctionCfiTombstoneFiltered(self):
    input_lines = [
        'STACK CFI INIT 0 50 .cfa: sp 0 + .ra: lr',  # Tombstone function.
        'STACK CFI 2 .cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
        'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^',
        'STACK CFI INIT 15b6490 4 .cfa: sp 0 + .ra: lr',
    ]

    f = io.StringIO(''.join(line + '\n' for line in input_lines))

    self.assertListEqual(
        [FunctionCfi(4, (AddressCfi(0x15b6490, '.cfa: sp 0 + .ra: lr'), ))],
        list(ReadFunctionCfi(f)))

  def testReadFunctionCfiSingleFunction(self):
    input_lines = [
        'STACK CFI INIT 15b6490 4 .cfa: sp 0 + .ra: lr',
        'STACK CFI 2 .cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
        'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^',
    ]

    f = io.StringIO(''.join(line + '\n' for line in input_lines))

    self.assertListEqual([
        FunctionCfi(4, (
            AddressCfi(0x15b6490, '.cfa: sp 0 + .ra: lr'),
            AddressCfi(
                0x2, '.cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
                'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^'),
        ))
    ], list(ReadFunctionCfi(f)))

  def testReadFunctionCfiMultipleFunctions(self):
    input_lines = [
        'STACK CFI INIT 15b6490 4 .cfa: sp 0 + .ra: lr',
        'STACK CFI 2 .cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
        'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^',
        'STACK CFI INIT 15b655a 26 .cfa: sp 0 + .ra: lr',
        'STACK CFI 15b655c .cfa: sp 8 + .ra: .cfa - 4 + ^ r4: .cfa - 8 + ^',
    ]

    f = io.StringIO(''.join(line + '\n' for line in input_lines))

    self.assertListEqual([
        FunctionCfi(0x4, (
            AddressCfi(0x15b6490, '.cfa: sp 0 + .ra: lr'),
            AddressCfi(
                0x2, '.cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
                'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^'),
        )),
        FunctionCfi(0x26, (
            AddressCfi(0x15b655a, '.cfa: sp 0 + .ra: lr'),
            AddressCfi(0x15b655c,
                       '.cfa: sp 8 + .ra: .cfa - 4 + ^ r4: .cfa - 8 + ^'),
        )),
    ], list(ReadFunctionCfi(f)))

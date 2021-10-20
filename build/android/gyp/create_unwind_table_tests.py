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

from create_unwind_table import (
    AddressCfi, AddressUnwind, FilterToNonTombstoneCfi, FunctionCfi,
    FunctionUnwind, EncodeAddressUnwind, EncodeAsCUbytes,
    EncodeStackPointerUpdate, EncodePop, NullParser, PushOrSubSpParser,
    ReadFunctionCfi, StoreSpParser, Uleb128Encode, UnwindType, VPushParser)


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


class _TestEncodeAsCUbytes(unittest.TestCase):
  def assertEncodingEqual(self, expected_values, encoded_c_ubytes):
    self.assertEqual(expected_values,
                     [ubyte.value for ubyte in encoded_c_ubytes])

  def testOutOfBounds(self):
    self.assertRaises(ValueError, lambda: EncodeAsCUbytes(1024))
    self.assertRaises(ValueError, lambda: EncodeAsCUbytes(256))
    self.assertRaises(ValueError, lambda: EncodeAsCUbytes(-1))

  def testEncode(self):
    self.assertEncodingEqual([0], EncodeAsCUbytes(0))
    self.assertEncodingEqual([255], EncodeAsCUbytes(255))
    self.assertEncodingEqual([0, 1], EncodeAsCUbytes(0, 1))


class _TestUleb128Encode(unittest.TestCase):
  def assertEncodingEqual(self, expected_values, encoded_c_ubytes):
    self.assertEqual(expected_values,
                     [ubyte.value for ubyte in encoded_c_ubytes])

  def testNegativeValue(self):
    self.assertRaises(ValueError, lambda: Uleb128Encode(-1))

  def testSingleByte(self):
    self.assertEncodingEqual([0], Uleb128Encode(0))
    self.assertEncodingEqual([1], Uleb128Encode(1))
    self.assertEncodingEqual([127], Uleb128Encode(127))

  def testMultiBytes(self):
    self.assertEncodingEqual([0b10000000, 0b1], Uleb128Encode(128))
    self.assertEncodingEqual([0b10000000, 0b10000000, 0b1],
                             Uleb128Encode(128**2))


class _TestEncodeStackPointerUpdate(unittest.TestCase):
  def assertEncodingEqual(self, expected_values, encoded_c_ubytes):
    self.assertEqual(expected_values,
                     [ubyte.value for ubyte in encoded_c_ubytes])

  def testSingleByte(self):
    self.assertEncodingEqual([0b00000000 | 0], EncodeStackPointerUpdate(4))
    self.assertEncodingEqual([0b01000000 | 0], EncodeStackPointerUpdate(-4))

    self.assertEncodingEqual([0b00000000 | 0b00111111],
                             EncodeStackPointerUpdate(0x100))
    self.assertEncodingEqual([0b01000000 | 0b00111111],
                             EncodeStackPointerUpdate(-0x100))

    self.assertEncodingEqual([0b00000000 | 3], EncodeStackPointerUpdate(16))
    self.assertEncodingEqual([0b01000000 | 3], EncodeStackPointerUpdate(-16))

    # 10110010 uleb128
    # vsp = vsp + 0x204 + (uleb128 << 2)
    self.assertEncodingEqual([0b10110010, 0b00000000],
                             EncodeStackPointerUpdate(0x204))
    self.assertEncodingEqual([0b10110010, 0b00000001],
                             EncodeStackPointerUpdate(0x208))

    # For vsp increments of 0x104-0x200, use 00xxxxxx twice.
    self.assertEncodingEqual([0b00111111, 0b00111111],
                             EncodeStackPointerUpdate(0x200))
    self.assertEncodingEqual([0b01111111, 0b01111111],
                             EncodeStackPointerUpdate(-0x200))

    # Not multiple of 4.
    self.assertRaises(AssertionError, lambda: EncodeStackPointerUpdate(101))
    # offset=0 is meaningless.
    self.assertRaises(AssertionError, lambda: EncodeStackPointerUpdate(0))


class _TestEncodePop(unittest.TestCase):
  def assertEncodingEqual(self, expected_values, encoded_c_ubytes):
    self.assertEqual(expected_values,
                     [ubyte.value for ubyte in encoded_c_ubytes])

  def testSingleRegister(self):
    # Should reject registers outside r4 ~ r15 range.
    for r in 0, 1, 2, 3, 16:
      self.assertRaises(AssertionError, lambda: EncodePop([r]))
    # Should use
    # 1000iiii iiiiiiii
    # Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
    self.assertEncodingEqual([0b10000000, 0b00000001], EncodePop([4]))
    self.assertEncodingEqual([0b10000000, 0b00001000], EncodePop([7]))
    self.assertEncodingEqual([0b10000100, 0b00000000], EncodePop([14]))
    self.assertEncodingEqual([0b10001000, 0b00000000], EncodePop([15]))

  def testContinuousRegisters(self):
    # 10101nnn
    # Pop r4-r[4+nnn], r14.
    self.assertEncodingEqual([0b10101000], EncodePop([4, 14]))
    self.assertEncodingEqual([0b10101001], EncodePop([4, 5, 14]))
    self.assertEncodingEqual([0b10101111],
                             EncodePop([4, 5, 6, 7, 8, 9, 10, 11, 14]))

  def testDiscontinuousRegisters(self):
    # 1000iiii iiiiiiii
    # Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
    self.assertEncodingEqual([0b10001000, 0b00000001], EncodePop([4, 15]))
    self.assertEncodingEqual([0b10000100, 0b00011000], EncodePop([7, 8, 14]))
    self.assertEncodingEqual([0b10000111, 0b11111111],
                             EncodePop([4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]))
    self.assertEncodingEqual([0b10000100, 0b10111111],
                             EncodePop([4, 5, 6, 7, 8, 9, 11, 14]))


class _TestEncodeAddressUnwind(unittest.TestCase):
  def assertEncodingEqual(self, expected_values, encoded_c_ubytes):
    self.assertEqual(expected_values,
                     [ubyte.value for ubyte in encoded_c_ubytes])

  def testReturnToLr(self):
    self.assertEncodingEqual([0b10110000],
                             EncodeAddressUnwind(
                                 AddressUnwind(
                                     address_offset=0,
                                     unwind_type=UnwindType.RETURN_TO_LR,
                                     sp_offset=0,
                                     registers=tuple())))

  def testNoAction(self):
    self.assertEncodingEqual([],
                             EncodeAddressUnwind(
                                 AddressUnwind(address_offset=0,
                                               unwind_type=UnwindType.NO_ACTION,
                                               sp_offset=0,
                                               registers=tuple())))

  def testUpdateSpAndOrPopRegisters(self):
    self.assertEncodingEqual(
        [0b0, 0b10101000],
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0x4,
                          registers=(4, 14))))

    self.assertEncodingEqual(
        [0b0],
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0x4,
                          registers=tuple())))

    self.assertEncodingEqual(
        [0b10101000],
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0,
                          registers=(4, 14))))

  def testRestoreSpFromRegisters(self):
    self.assertEncodingEqual(
        [0b10010100, 0b0],
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.RESTORE_SP_FROM_REGISTER,
                          sp_offset=0x4,
                          registers=(4, ))))

    self.assertEncodingEqual(
        [0b10010100],
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.RESTORE_SP_FROM_REGISTER,
                          sp_offset=0,
                          registers=(4, ))))

    self.assertRaises(
        AssertionError, lambda: EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.RESTORE_SP_FROM_REGISTER,
                          sp_offset=0x4,
                          registers=tuple())))


class _TestNullParser(unittest.TestCase):
  def testCfaChange(self):
    parser = NullParser()
    match = parser.GetBreakpadInstructionsRegex().search('.cfa: sp 0 + .ra: lr')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=0,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(0, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=0,
                      unwind_type=UnwindType.RETURN_TO_LR,
                      sp_offset=0,
                      registers=()), address_unwind)


class _TestPushOrSubSpParser(unittest.TestCase):
  def testCfaChange(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search('.cfa: sp 4 +')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(4, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=4,
                      registers=()), address_unwind)

  def testCfaAndRaChangePopOnly(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 4 + .ra: .cfa -4 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(4, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=0,
                      registers=(14, )), address_unwind)

  def testCfaAndRaChangePopAndSpUpdate(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 8 + .ra: .cfa -4 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(8, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=4,
                      registers=(14, )), address_unwind)

  def testCfaAndRaAndRegistersChangePopOnly(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 12 + .ra: .cfa -4 + ^ r4: .cfa -12 + ^ r7: .cfa -8 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(12, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=0,
                      registers=(4, 7, 14)), address_unwind)

  def testCfaAndRaAndRegistersChangePopAndSpUpdate(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 16 + .ra: .cfa -4 + ^ r4: .cfa -12 + ^ r7: .cfa -8 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(16, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=4,
                      registers=(4, 7, 14)), address_unwind)

  def testRegistersChange(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        'r4: .cfa -8 + ^ r7: .cfa -4 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(0, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=0,
                      registers=(4, 7)), address_unwind)

  def testCfaAndRegistersChange(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 8 + r4: .cfa -8 + ^ r7: .cfa -4 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(8, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=0,
                      registers=(4, 7)), address_unwind)

  def testRegistersOrdering(self):
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        'r10: .cfa -8 + ^ r7: .cfa -4 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(0, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=0,
                      registers=(7, 10)), address_unwind)


class _TestVPushParser(unittest.TestCase):
  def testCfaAndRegistersChange(self):
    parser = VPushParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 40 + unnamed_register264: .cfa -40 + ^ '
        'unnamed_register265: .cfa -32 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=24,
                                                              match=match)

    self.assertEqual(40, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=16,
                      registers=()), address_unwind)

  def testRegistersChange(self):
    parser = VPushParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        'unnamed_register264: .cfa -40 + ^ unnamed_register265: .cfa -32 + ^')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=24,
                                                              match=match)

    self.assertEqual(24, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.NO_ACTION,
                      sp_offset=0,
                      registers=()), address_unwind)


class _TestStoreSpParser(unittest.TestCase):
  def testCfaAndRegistersChange(self):
    parser = StoreSpParser()
    match = parser.GetBreakpadInstructionsRegex().search('.cfa: r7 8 +')
    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=12,
                                                              match=match)

    self.assertEqual(8, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.RESTORE_SP_FROM_REGISTER,
                      sp_offset=-4,
                      registers=(7, )), address_unwind)

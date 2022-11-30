#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for create_unwind_table.py.

This test suite contains tests for the custom unwind table creation for 32-bit
arm builds.
"""

import io
import struct

import unittest
import unittest.mock
import re

from create_unwind_table import (
    AddressCfi, AddressUnwind, FilterToNonTombstoneCfi, FunctionCfi,
    FunctionUnwind, EncodeAddressUnwind, EncodeAddressUnwinds,
    EncodedAddressUnwind, EncodeAsBytes, EncodeFunctionOffsetTable,
    EncodedFunctionUnwind, EncodeFunctionUnwinds, EncodeStackPointerUpdate,
    EncodePop, EncodePageTableAndFunctionTable, EncodeUnwindInfo,
    EncodeUnwindInstructionTable, GenerateUnwinds, GenerateUnwindTables,
    NullParser, ParseAddressCfi, PushOrSubSpParser, ReadFunctionCfi,
    REFUSE_TO_UNWIND, StoreSpParser, TRIVIAL_UNWIND, Uleb128Encode,
    UnwindInstructionsParser, UnwindType, VPushParser)


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

    self.assertEqual([
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

    self.assertEqual(
        [FunctionCfi(4, (AddressCfi(0x15b6490, '.cfa: sp 0 + .ra: lr'), ))],
        list(ReadFunctionCfi(f)))

  def testReadFunctionCfiSingleFunction(self):
    input_lines = [
        'STACK CFI INIT 15b6490 4 .cfa: sp 0 + .ra: lr',
        'STACK CFI 2 .cfa: sp 24 + .ra: .cfa - 4 + ^ r4: .cfa - 16 + ^ '
        'r5: .cfa - 12 + ^ r7: .cfa - 8 + ^',
    ]

    f = io.StringIO(''.join(line + '\n' for line in input_lines))

    self.assertEqual([
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

    self.assertEqual([
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


class _TestEncodeAsBytes(unittest.TestCase):
  def testOutOfBounds(self):
    self.assertRaises(ValueError, lambda: EncodeAsBytes(1024))
    self.assertRaises(ValueError, lambda: EncodeAsBytes(256))
    self.assertRaises(ValueError, lambda: EncodeAsBytes(-1))

  def testEncode(self):
    self.assertEqual(bytes([0]), EncodeAsBytes(0))
    self.assertEqual(bytes([255]), EncodeAsBytes(255))
    self.assertEqual(bytes([0, 1]), EncodeAsBytes(0, 1))


class _TestUleb128Encode(unittest.TestCase):
  def testNegativeValue(self):
    self.assertRaises(ValueError, lambda: Uleb128Encode(-1))

  def testSingleByte(self):
    self.assertEqual(bytes([0]), Uleb128Encode(0))
    self.assertEqual(bytes([1]), Uleb128Encode(1))
    self.assertEqual(bytes([127]), Uleb128Encode(127))

  def testMultiBytes(self):
    self.assertEqual(bytes([0b10000000, 0b1]), Uleb128Encode(128))
    self.assertEqual(bytes([0b10000000, 0b10000000, 0b1]),
                     Uleb128Encode(128**2))


class _TestEncodeStackPointerUpdate(unittest.TestCase):
  def testSingleByte(self):
    self.assertEqual(bytes([0b00000000 | 0]), EncodeStackPointerUpdate(4))
    self.assertEqual(bytes([0b01000000 | 0]), EncodeStackPointerUpdate(-4))

    self.assertEqual(bytes([0b00000000 | 0b00111111]),
                     EncodeStackPointerUpdate(0x100))
    self.assertEqual(bytes([0b01000000 | 0b00111111]),
                     EncodeStackPointerUpdate(-0x100))

    self.assertEqual(bytes([0b00000000 | 3]), EncodeStackPointerUpdate(16))
    self.assertEqual(bytes([0b01000000 | 3]), EncodeStackPointerUpdate(-16))

    self.assertEqual(bytes([0b00111111]), EncodeStackPointerUpdate(0x100))

    # 10110010 uleb128
    # vsp = vsp + 0x204 + (uleb128 << 2)
    self.assertEqual(bytes([0b10110010, 0b00000000]),
                     EncodeStackPointerUpdate(0x204))
    self.assertEqual(bytes([0b10110010, 0b00000001]),
                     EncodeStackPointerUpdate(0x208))

    # For vsp increments of 0x104-0x200, use 00xxxxxx twice.
    self.assertEqual(bytes([0b00111111, 0b00000000]),
                     EncodeStackPointerUpdate(0x104))
    self.assertEqual(bytes([0b00111111, 0b00111111]),
                     EncodeStackPointerUpdate(0x200))
    self.assertEqual(bytes([0b01111111, 0b01111111]),
                     EncodeStackPointerUpdate(-0x200))

    # Not multiple of 4.
    self.assertRaises(AssertionError, lambda: EncodeStackPointerUpdate(101))
    # offset=0 is meaningless.
    self.assertRaises(AssertionError, lambda: EncodeStackPointerUpdate(0))


class _TestEncodePop(unittest.TestCase):
  def testSingleRegister(self):
    # Should reject registers outside r4 ~ r15 range.
    for r in 0, 1, 2, 3, 16:
      self.assertRaises(AssertionError, lambda: EncodePop([r]))
    # Should use
    # 1000iiii iiiiiiii
    # Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
    self.assertEqual(bytes([0b10000000, 0b00000001]), EncodePop([4]))
    self.assertEqual(bytes([0b10000000, 0b00001000]), EncodePop([7]))
    self.assertEqual(bytes([0b10000100, 0b00000000]), EncodePop([14]))
    self.assertEqual(bytes([0b10001000, 0b00000000]), EncodePop([15]))

  def testContinuousRegisters(self):
    # 10101nnn
    # Pop r4-r[4+nnn], r14.
    self.assertEqual(bytes([0b10101000]), EncodePop([4, 14]))
    self.assertEqual(bytes([0b10101001]), EncodePop([4, 5, 14]))
    self.assertEqual(bytes([0b10101111]),
                     EncodePop([4, 5, 6, 7, 8, 9, 10, 11, 14]))

  def testDiscontinuousRegisters(self):
    # 1000iiii iiiiiiii
    # Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
    self.assertEqual(bytes([0b10001000, 0b00000001]), EncodePop([4, 15]))
    self.assertEqual(bytes([0b10000100, 0b00011000]), EncodePop([7, 8, 14]))
    self.assertEqual(bytes([0b10000111, 0b11111111]),
                     EncodePop([4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]))
    self.assertEqual(bytes([0b10000100, 0b10111111]),
                     EncodePop([4, 5, 6, 7, 8, 9, 11, 14]))


class _TestEncodeAddressUnwind(unittest.TestCase):
  def testReturnToLr(self):
    self.assertEqual(
        bytes([0b10110000]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.RETURN_TO_LR,
                          sp_offset=0,
                          registers=tuple())))

  def testNoAction(self):
    self.assertEqual(
        bytes([]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.NO_ACTION,
                          sp_offset=0,
                          registers=tuple())))

  def testUpdateSpAndOrPopRegisters(self):
    self.assertEqual(
        bytes([0b0, 0b10101000]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0x4,
                          registers=(4, 14))))

    self.assertEqual(
        bytes([0b0]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0x4,
                          registers=tuple())))

    self.assertEqual(
        bytes([0b10101000]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                          sp_offset=0,
                          registers=(4, 14))))

  def testRestoreSpFromRegisters(self):
    self.assertEqual(
        bytes([0b10010100, 0b0]),
        EncodeAddressUnwind(
            AddressUnwind(address_offset=0,
                          unwind_type=UnwindType.RESTORE_SP_FROM_REGISTER,
                          sp_offset=0x4,
                          registers=(4, ))))

    self.assertEqual(
        bytes([0b10010100]),
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


class _TestEncodeAddressUnwinds(unittest.TestCase):
  def testEncodeOrder(self):
    address_unwind1 = AddressUnwind(address_offset=0,
                                    unwind_type=UnwindType.RETURN_TO_LR,
                                    sp_offset=0,
                                    registers=tuple())
    address_unwind2 = AddressUnwind(
        address_offset=4,
        unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
        sp_offset=0,
        registers=(4, 14))

    def MockEncodeAddressUnwind(address_unwind):
      return {
          address_unwind1: bytes([1]),
          address_unwind2: bytes([2]),
      }[address_unwind]

    with unittest.mock.patch("create_unwind_table.EncodeAddressUnwind",
                             side_effect=MockEncodeAddressUnwind):
      encoded_unwinds = EncodeAddressUnwinds((address_unwind1, address_unwind2))
      self.assertEqual((
          EncodedAddressUnwind(4,
                               bytes([2]) + bytes([1])),
          EncodedAddressUnwind(0, bytes([1])),
      ), encoded_unwinds)


PAGE_SIZE = 1 << 17


class _TestEncodeFunctionUnwinds(unittest.TestCase):
  @unittest.mock.patch('create_unwind_table.EncodeAddressUnwinds')
  def testEncodeOrder(self, MockEncodeAddressUnwinds):
    MockEncodeAddressUnwinds.return_value = EncodedAddressUnwind(0, b'\x00')

    self.assertEqual([
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0,
                              address_unwinds=EncodedAddressUnwind(0, b'\x00')),
        EncodedFunctionUnwind(page_number=0,
                              page_offset=100 >> 1,
                              address_unwinds=EncodedAddressUnwind(0, b'\x00')),
    ],
                     list(
                         EncodeFunctionUnwinds([
                             FunctionUnwind(address=100,
                                            size=PAGE_SIZE - 100,
                                            address_unwinds=()),
                             FunctionUnwind(
                                 address=0, size=100, address_unwinds=()),
                         ],
                                               text_section_start_address=0)))

  @unittest.mock.patch('create_unwind_table.EncodeAddressUnwinds')
  def testFillingGaps(self, MockEncodeAddressUnwinds):
    MockEncodeAddressUnwinds.return_value = EncodedAddressUnwind(0, b'\x00')

    self.assertEqual([
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0,
                              address_unwinds=EncodedAddressUnwind(0, b'\x00')),
        EncodedFunctionUnwind(
            page_number=0, page_offset=50 >> 1, address_unwinds=TRIVIAL_UNWIND),
        EncodedFunctionUnwind(page_number=0,
                              page_offset=100 >> 1,
                              address_unwinds=EncodedAddressUnwind(0, b'\x00')),
    ],
                     list(
                         EncodeFunctionUnwinds([
                             FunctionUnwind(
                                 address=0, size=50, address_unwinds=()),
                             FunctionUnwind(address=100,
                                            size=PAGE_SIZE - 100,
                                            address_unwinds=()),
                         ],
                                               text_section_start_address=0)))

  @unittest.mock.patch('create_unwind_table.EncodeAddressUnwinds')
  def testFillingLastPage(self, MockEncodeAddressUnwinds):
    MockEncodeAddressUnwinds.return_value = EncodedAddressUnwind(0, b'\x00')

    self.assertEqual(
        [
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=0,
                                  address_unwinds=EncodedAddressUnwind(
                                      0, b'\x00')),
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=100 >> 1,
                                  address_unwinds=EncodedAddressUnwind(
                                      0, b'\x00')),
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=200 >> 1,
                                  address_unwinds=REFUSE_TO_UNWIND),
        ],
        list(
            EncodeFunctionUnwinds([
                FunctionUnwind(address=1100, size=100, address_unwinds=()),
                FunctionUnwind(address=1200, size=100, address_unwinds=()),
            ],
                                  text_section_start_address=1100)))

  @unittest.mock.patch('create_unwind_table.EncodeAddressUnwinds')
  def testFillingFirstPage(self, MockEncodeAddressUnwinds):
    MockEncodeAddressUnwinds.return_value = EncodedAddressUnwind(0, b'\x00')

    self.assertEqual(
        [
            EncodedFunctionUnwind(
                page_number=0, page_offset=0, address_unwinds=REFUSE_TO_UNWIND),
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=100 >> 1,
                                  address_unwinds=EncodedAddressUnwind(
                                      0, b'\x00')),
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=200 >> 1,
                                  address_unwinds=EncodedAddressUnwind(
                                      0, b'\x00')),
            EncodedFunctionUnwind(page_number=0,
                                  page_offset=300 >> 1,
                                  address_unwinds=REFUSE_TO_UNWIND),
        ],
        list(
            EncodeFunctionUnwinds([
                FunctionUnwind(address=1100, size=100, address_unwinds=()),
                FunctionUnwind(address=1200, size=100, address_unwinds=()),
            ],
                                  text_section_start_address=1000)))

  @unittest.mock.patch('create_unwind_table.EncodeAddressUnwinds')
  def testOverlappedFunctions(self, _):
    self.assertRaises(
        # Eval generator with `list`. Otherwise the code will not execute.
        AssertionError,
        lambda: list(
            EncodeFunctionUnwinds([
                FunctionUnwind(address=0, size=100, address_unwinds=()),
                FunctionUnwind(address=50, size=100, address_unwinds=()),
            ],
                                  text_section_start_address=0)))


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

  def testPoppingCallerSaveRegisters(self):
    """Regression test for pop unwinds that encode caller-save registers.

    Callee-save registers: r0 ~ r3.
    """
    parser = PushOrSubSpParser()
    match = parser.GetBreakpadInstructionsRegex().search(
        '.cfa: sp 16 + .ra: .cfa -4 + ^ '
        'r3: .cfa -16 + ^ r4: .cfa -12 + ^ r5: .cfa -8 + ^')

    self.assertIsNotNone(match)

    address_unwind, new_cfa_sp_offset = parser.ParseFromMatch(address_offset=20,
                                                              cfa_sp_offset=0,
                                                              match=match)

    self.assertEqual(16, new_cfa_sp_offset)
    self.assertEqual(
        AddressUnwind(address_offset=20,
                      unwind_type=UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS,
                      sp_offset=4,
                      registers=(4, 5, 14)), address_unwind)


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


class _TestEncodeUnwindInstructionTable(unittest.TestCase):
  def testSingleEntry(self):
    table, offsets = EncodeUnwindInstructionTable([bytes([3])])

    self.assertEqual(bytes([3]), table)
    self.assertDictEqual({
        bytes([3]): 0,
    }, offsets)

  def testMultipleEntries(self):
    self.maxDiff = None
    # Result should be sorted by score descending.
    table, offsets = EncodeUnwindInstructionTable([
        bytes([1, 2, 3]),
        bytes([0, 3]),
        bytes([3]),
    ])
    self.assertEqual(bytes([3, 0, 3, 1, 2, 3]), table)
    self.assertDictEqual(
        {
            bytes([1, 2, 3]): 3,  # score = 1 / 3 = 0.67
            bytes([0, 3]): 1,  # score = 1 / 2 = 0.5
            bytes([3]): 0,  # score = 1 / 1 = 1
        },
        offsets)

    # When scores are same, sort by sequence descending.
    table, offsets = EncodeUnwindInstructionTable([
        bytes([3]),
        bytes([0, 3]),
        bytes([0, 3]),
        bytes([1, 2, 3]),
        bytes([1, 2, 3]),
        bytes([1, 2, 3]),
    ])
    self.assertEqual(bytes([3, 1, 2, 3, 0, 3]), table)
    self.assertDictEqual(
        {
            bytes([3]): 0,  # score = 1 / 1 = 1
            bytes([1, 2, 3]): 1,  # score = 3 / 3 = 1
            bytes([0, 3]): 4,  # score = 2 / 2 = 1
        },
        offsets)


class _TestFunctionOffsetTable(unittest.TestCase):
  def testSingleEntry(self):
    self.maxDiff = None
    complete_instruction_sequence0 = bytes([3])
    complete_instruction_sequence1 = bytes([1, 3])

    sequence1 = (
        EncodedAddressUnwind(0x400, complete_instruction_sequence1),
        EncodedAddressUnwind(0x0, complete_instruction_sequence0),
    )

    address_unwind_sequences = [sequence1]

    table, offsets = EncodeFunctionOffsetTable(
        address_unwind_sequences, {
            complete_instruction_sequence0: 52,
            complete_instruction_sequence1: 50,
        })

    self.assertEqual(
        bytes([
            # (0x200, 50)
            128,
            4,
            50,
            # (0, 52)
            0,
            52,
        ]),
        table)

    self.assertDictEqual({
        sequence1: 0,
    }, offsets)

  def testMultipleEntry(self):
    self.maxDiff = None
    complete_instruction_sequence0 = bytes([3])
    complete_instruction_sequence1 = bytes([1, 3])
    complete_instruction_sequence2 = bytes([2, 3])

    sequence1 = (
        EncodedAddressUnwind(0x20, complete_instruction_sequence1),
        EncodedAddressUnwind(0x0, complete_instruction_sequence0),
    )
    sequence2 = (
        EncodedAddressUnwind(0x400, complete_instruction_sequence2),
        EncodedAddressUnwind(0x0, complete_instruction_sequence0),
    )
    address_unwind_sequences = [sequence1, sequence2]

    table, offsets = EncodeFunctionOffsetTable(
        address_unwind_sequences, {
            complete_instruction_sequence0: 52,
            complete_instruction_sequence1: 50,
            complete_instruction_sequence2: 80,
        })

    self.assertEqual(
        bytes([
            # (0x10, 50)
            0x10,
            50,
            # (0, 52)
            0,
            52,
            # (0x200, 80)
            128,
            4,
            80,
            # (0, 52)
            0,
            52,
        ]),
        table)

    self.assertDictEqual({
        sequence1: 0,
        sequence2: 4,
    }, offsets)

  def testDuplicatedEntry(self):
    self.maxDiff = None
    complete_instruction_sequence0 = bytes([3])
    complete_instruction_sequence1 = bytes([1, 3])
    complete_instruction_sequence2 = bytes([2, 3])

    sequence1 = (
        EncodedAddressUnwind(0x20, complete_instruction_sequence1),
        EncodedAddressUnwind(0x0, complete_instruction_sequence0),
    )
    sequence2 = (
        EncodedAddressUnwind(0x400, complete_instruction_sequence2),
        EncodedAddressUnwind(0x0, complete_instruction_sequence0),
    )
    sequence3 = sequence1

    address_unwind_sequences = [sequence1, sequence2, sequence3]

    table, offsets = EncodeFunctionOffsetTable(
        address_unwind_sequences, {
            complete_instruction_sequence0: 52,
            complete_instruction_sequence1: 50,
            complete_instruction_sequence2: 80,
        })

    self.assertEqual(
        bytes([
            # (0x10, 50)
            0x10,
            50,
            # (0, 52)
            0,
            52,
            # (0x200, 80)
            128,
            4,
            80,
            # (0, 52)
            0,
            52,
        ]),
        table)

    self.assertDictEqual({
        sequence1: 0,
        sequence2: 4,
    }, offsets)


class _TestEncodePageTableAndFunctionTable(unittest.TestCase):
  def testMultipleFunctionUnwinds(self):
    address_unwind_sequence0 = (
        EncodedAddressUnwind(0x10, bytes([0, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )
    address_unwind_sequence1 = (
        EncodedAddressUnwind(0x10, bytes([1, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )
    address_unwind_sequence2 = (
        EncodedAddressUnwind(0x200, bytes([2, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )

    function_unwinds = [
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0,
                              address_unwinds=address_unwind_sequence0),
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0x8000,
                              address_unwinds=address_unwind_sequence1),
        EncodedFunctionUnwind(page_number=1,
                              page_offset=0x8000,
                              address_unwinds=address_unwind_sequence2),
    ]

    function_offset_table_offsets = {
        address_unwind_sequence0: 0x100,
        address_unwind_sequence1: 0x200,
        address_unwind_sequence2: 0x300,
    }

    page_table, function_table = EncodePageTableAndFunctionTable(
        function_unwinds, function_offset_table_offsets)

    self.assertEqual(2 * 4, len(page_table))
    self.assertEqual((0, 2), struct.unpack('2I', page_table))

    self.assertEqual(6 * 2, len(function_table))
    self.assertEqual((0, 0x100, 0x8000, 0x200, 0x8000, 0x300),
                     struct.unpack('6H', function_table))

  def testMultiPageFunction(self):
    address_unwind_sequence0 = (
        EncodedAddressUnwind(0x10, bytes([0, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )
    address_unwind_sequence1 = (
        EncodedAddressUnwind(0x10, bytes([1, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )
    address_unwind_sequence2 = (
        EncodedAddressUnwind(0x200, bytes([2, 3])),
        EncodedAddressUnwind(0x0, bytes([3])),
    )

    function_unwinds = [
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0,
                              address_unwinds=address_unwind_sequence0),
        # Large function.
        EncodedFunctionUnwind(page_number=0,
                              page_offset=0x8000,
                              address_unwinds=address_unwind_sequence1),
        EncodedFunctionUnwind(page_number=4,
                              page_offset=0x8000,
                              address_unwinds=address_unwind_sequence2),
    ]

    function_offset_table_offsets = {
        address_unwind_sequence0: 0x100,
        address_unwind_sequence1: 0x200,
        address_unwind_sequence2: 0x300,
    }

    page_table, function_table = EncodePageTableAndFunctionTable(
        function_unwinds, function_offset_table_offsets)

    self.assertEqual(5 * 4, len(page_table))
    self.assertEqual((0, 2, 2, 2, 2), struct.unpack('5I', page_table))

    self.assertEqual(6 * 2, len(function_table))
    self.assertEqual((0, 0x100, 0x8000, 0x200, 0x8000, 0x300),
                     struct.unpack('6H', function_table))


class MockReturnParser(UnwindInstructionsParser):
  def GetBreakpadInstructionsRegex(self):
    return re.compile(r'^RETURN$')

  def ParseFromMatch(self, address_offset, cfa_sp_offset, match):
    return AddressUnwind(address_offset, UnwindType.RETURN_TO_LR, 0, ()), 0


class MockEpilogueUnwindParser(UnwindInstructionsParser):
  def GetBreakpadInstructionsRegex(self):
    return re.compile(r'^EPILOGUE_UNWIND$')

  def ParseFromMatch(self, address_offset, cfa_sp_offset, match):
    return AddressUnwind(address_offset,
                         UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS, 0, ()), -100


class MockWildcardParser(UnwindInstructionsParser):
  def GetBreakpadInstructionsRegex(self):
    return re.compile(r'.*')

  def ParseFromMatch(self, address_offset, cfa_sp_offset, match):
    return AddressUnwind(address_offset,
                         UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS, 0, ()), -200


class _TestParseAddressCfi(unittest.TestCase):
  def testSuccessParse(self):
    address_unwind = AddressUnwind(
        address_offset=0x300,
        unwind_type=UnwindType.RETURN_TO_LR,
        sp_offset=0,
        registers=(),
    )

    self.assertEqual((address_unwind, False, 0),
                     ParseAddressCfi(AddressCfi(address=0x800,
                                                unwind_instructions='RETURN'),
                                     function_start_address=0x500,
                                     parsers=(MockReturnParser(), ),
                                     prev_cfa_sp_offset=0))

  def testUnhandledAddress(self):
    self.assertEqual((None, False, 100),
                     ParseAddressCfi(AddressCfi(address=0x800,
                                                unwind_instructions='UNKNOWN'),
                                     function_start_address=0x500,
                                     parsers=(MockReturnParser(), ),
                                     prev_cfa_sp_offset=100))

  def testEpilogueUnwind(self):
    self.assertEqual(
        (None, True, -100),
        ParseAddressCfi(AddressCfi(address=0x800,
                                   unwind_instructions='EPILOGUE_UNWIND'),
                        function_start_address=0x500,
                        parsers=(MockEpilogueUnwindParser(), ),
                        prev_cfa_sp_offset=100))

  def testParsePrecedence(self):
    address_unwind = AddressUnwind(
        address_offset=0x300,
        unwind_type=UnwindType.RETURN_TO_LR,
        sp_offset=0,
        registers=(),
    )

    self.assertEqual(
        (address_unwind, False, 0),
        ParseAddressCfi(AddressCfi(address=0x800, unwind_instructions='RETURN'),
                        function_start_address=0x500,
                        parsers=(MockReturnParser(), MockWildcardParser()),
                        prev_cfa_sp_offset=0))


class _TestGenerateUnwinds(unittest.TestCase):
  def testSuccessUnwind(self):
    self.assertEqual(
        [
            FunctionUnwind(address=0x100,
                           size=1024,
                           address_unwinds=(
                               AddressUnwind(
                                   address_offset=0x0,
                                   unwind_type=UnwindType.RETURN_TO_LR,
                                   sp_offset=0,
                                   registers=(),
                               ),
                               AddressUnwind(
                                   address_offset=0x200,
                                   unwind_type=UnwindType.RETURN_TO_LR,
                                   sp_offset=0,
                                   registers=(),
                               ),
                           ))
        ],
        list(
            GenerateUnwinds([
                FunctionCfi(
                    size=1024,
                    address_cfi=(
                        AddressCfi(address=0x100, unwind_instructions='RETURN'),
                        AddressCfi(address=0x300, unwind_instructions='RETURN'),
                    ))
            ],
                            parsers=[MockReturnParser()])))

  def testUnhandledAddress(self):
    self.assertEqual(
        [
            FunctionUnwind(address=0x100,
                           size=1024,
                           address_unwinds=(AddressUnwind(
                               address_offset=0x0,
                               unwind_type=UnwindType.RETURN_TO_LR,
                               sp_offset=0,
                               registers=(),
                           ), ))
        ],
        list(
            GenerateUnwinds([
                FunctionCfi(size=1024,
                            address_cfi=(
                                AddressCfi(address=0x100,
                                           unwind_instructions='RETURN'),
                                AddressCfi(address=0x300,
                                           unwind_instructions='UNKNOWN'),
                            ))
            ],
                            parsers=[MockReturnParser()])))

  def testEpilogueUnwind(self):
    self.assertEqual(
        [
            FunctionUnwind(address=0x100,
                           size=1024,
                           address_unwinds=(AddressUnwind(
                               address_offset=0x0,
                               unwind_type=UnwindType.RETURN_TO_LR,
                               sp_offset=0,
                               registers=(),
                           ), ))
        ],
        list(
            GenerateUnwinds([
                FunctionCfi(
                    size=1024,
                    address_cfi=(
                        AddressCfi(address=0x100, unwind_instructions='RETURN'),
                        AddressCfi(address=0x300,
                                   unwind_instructions='EPILOGUE_UNWIND'),
                    ))
            ],
                            parsers=[
                                MockReturnParser(),
                                MockEpilogueUnwindParser()
                            ])))

  def testInvalidInitialUnwindInstructionAsserts(self):
    self.assertRaises(
        AssertionError, lambda: list(
            GenerateUnwinds([
                FunctionCfi(size=1024,
                            address_cfi=(
                                AddressCfi(address=0x100,
                                           unwind_instructions='UNKNOWN'),
                                AddressCfi(address=0x200,
                                           unwind_instructions='RETURN'),
                            ))
            ],
                            parsers=[MockReturnParser()])))


class _TestEncodeUnwindInfo(unittest.TestCase):
  def testEncodeTables(self):
    page_table = struct.pack('I', 0)
    function_table = struct.pack('4H', 1, 2, 3, 4)
    function_offset_table = bytes([1, 2])
    unwind_instruction_table = bytes([1, 2, 3])

    unwind_info = EncodeUnwindInfo(
        page_table,
        function_table,
        function_offset_table,
        unwind_instruction_table,
    )

    self.assertEqual(
        32 + len(page_table) + len(function_table) +
        len(function_offset_table) + len(unwind_instruction_table),
        len(unwind_info))
    # Header.
    self.assertEqual((32, 1, 36, 2, 44, 2, 46, 3),
                     struct.unpack('8I', unwind_info[:32]))
    # Body.
    self.assertEqual(
        page_table + function_table + function_offset_table +
        unwind_instruction_table, unwind_info[32:])

  def testUnalignedTables(self):
    self.assertRaises(
        AssertionError, lambda: EncodeUnwindInfo(bytes([1]), b'', b'', b''))
    self.assertRaises(
        AssertionError, lambda: EncodeUnwindInfo(b'', bytes([1]), b'', b''))


class _TestGenerateUnwindTables(unittest.TestCase):
  def testGenerateUnwindTables(self):
    """This is an integration test that hooks everything together. """
    address_unwind_sequence0 = (
        EncodedAddressUnwind(0x20, bytes([0, 0xb0])),
        EncodedAddressUnwind(0x0, bytes([0xb0])),
    )
    address_unwind_sequence1 = (
        EncodedAddressUnwind(0x20, bytes([1, 0xb0])),
        EncodedAddressUnwind(0x0, bytes([0xb0])),
    )
    address_unwind_sequence2 = (
        EncodedAddressUnwind(0x200, bytes([2, 0xb0])),
        EncodedAddressUnwind(0x0, bytes([0xb0])),
    )

    (page_table, function_table, function_offset_table,
     unwind_instruction_table) = GenerateUnwindTables([
         EncodedFunctionUnwind(page_number=0,
                               page_offset=0,
                               address_unwinds=TRIVIAL_UNWIND),
         EncodedFunctionUnwind(page_number=0,
                               page_offset=0x1000,
                               address_unwinds=address_unwind_sequence0),
         EncodedFunctionUnwind(page_number=1,
                               page_offset=0x2000,
                               address_unwinds=address_unwind_sequence1),
         EncodedFunctionUnwind(page_number=3,
                               page_offset=0x1000,
                               address_unwinds=address_unwind_sequence2),
     ])

    # Complete instruction sequences and their frequencies.
    # [0xb0]: 4
    # [0, 0xb0]: 1
    # [1, 0xb0]: 1
    # [2, 0xb0]: 1
    self.assertEqual(bytes([0xb0, 2, 0xb0, 1, 0xb0, 0, 0xb0]),
                     unwind_instruction_table)

    self.assertEqual(
        bytes([
            # Trivial unwind.
            0,
            0,
            # Address unwind sequence 0.
            0x10,
            5,
            0,
            0,
            # Address unwind sequence 1.
            0x10,
            3,
            0,
            0,
            # Address unwind sequence 2.
            0x80,
            2,
            1,
            0,
            0,
        ]),
        function_offset_table)

    self.assertEqual(8 * 2, len(function_table))
    self.assertEqual((0, 0, 0x1000, 2, 0x2000, 6, 0x1000, 10),
                     struct.unpack('8H', function_table))

    self.assertEqual(4 * 4, len(page_table))
    self.assertEqual((0, 2, 3, 3), struct.unpack('4I', page_table))

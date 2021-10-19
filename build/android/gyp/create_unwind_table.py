#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a table of unwind information in Android Chrome's bespoke format."""

import ctypes
import enum
import re
from typing import Iterable, List, NamedTuple, Sequence, TextIO, Tuple

_STACK_CFI_INIT_REGEX = re.compile(
    r'^STACK CFI INIT ([0-9a-f]+) ([0-9a-f]+) (.+)$')
_STACK_CFI_REGEX = re.compile(r'^STACK CFI ([0-9a-f]+) (.+)$')


class AddressCfi(NamedTuple):
  """Record representing CFI for an address within a function.

  Represents the Call Frame Information required to unwind from an address in a
  function.

  Attributes:
      address: The address.
      unwind_instructions: The unwind instructions for the address.

  """
  address: int
  unwind_instructions: str


class FunctionCfi(NamedTuple):
  """Record representing CFI for a function.

  Note: address_cfi[0].address is the start address of the function.

  Attributes:
      size: The function size in bytes.
      address_cfi: The CFI at each address in the function.

  """
  size: int
  address_cfi: Tuple[AddressCfi, ...]


def FilterToNonTombstoneCfi(stream: TextIO) -> Iterable[str]:
  """Generates non-tombstone STACK CFI lines from the stream.

  STACK CFI functions with address 0 correspond are a 'tombstone' record
  associated with dead code and can be ignored. See
  https://bugs.llvm.org/show_bug.cgi?id=47148#c2.

  Args:
      stream: A file object.

  Returns:
      An iterable over the non-tombstone STACK CFI lines in the stream.
  """
  in_tombstone_function = False
  for line in stream:
    if not line.startswith('STACK CFI '):
      continue

    if line.startswith('STACK CFI INIT 0 '):
      in_tombstone_function = True
    elif line.startswith('STACK CFI INIT '):
      in_tombstone_function = False

    if not in_tombstone_function:
      yield line


def ReadFunctionCfi(stream: TextIO) -> Iterable[FunctionCfi]:
  """Generates FunctionCfi records from the stream.

  Args:
      stream: A file object.

  Returns:
      An iterable over FunctionCfi corresponding to the non-tombstone STACK CFI
      lines in the stream.
  """
  current_function_address = None
  current_function_size = None
  current_function_address_cfi = []
  for line in FilterToNonTombstoneCfi(stream):
    cfi_init_match = _STACK_CFI_INIT_REGEX.search(line)
    if cfi_init_match:
      # Function CFI with address 0 are tombstone entries per
      # https://bugs.llvm.org/show_bug.cgi?id=47148#c2 and should have been
      # filtered in `FilterToNonTombstoneCfi`.
      assert current_function_address != 0
      if (current_function_address is not None
          and current_function_size is not None):
        yield FunctionCfi(current_function_size,
                          tuple(current_function_address_cfi))
      current_function_address = int(cfi_init_match.group(1), 16)
      current_function_size = int(cfi_init_match.group(2), 16)
      current_function_address_cfi = [
          AddressCfi(int(cfi_init_match.group(1), 16), cfi_init_match.group(3))
      ]
    else:
      cfi_match = _STACK_CFI_REGEX.search(line)
      assert cfi_match
      current_function_address_cfi.append(
          AddressCfi(int(cfi_match.group(1), 16), cfi_match.group(2)))

  assert current_function_address is not None
  assert current_function_size is not None
  yield FunctionCfi(current_function_size, tuple(current_function_address_cfi))


def EncodeAsCUbytes(*values: int) -> List[ctypes.c_ubyte]:
  """Encodes the argument ints as a list of ctypes.c_ubyte.

  We encode as ctypes.c_ubyte because that type is guaranteed to be one byte in
  size when encoded as binary. This function validates that the inputs are
  within the range that can be represented as bytes.

  Argument:
    values: Integers in range [0, 255].

  Returns:
    List of c_ubyte.
  """
  for i, value in enumerate(values):
    if not 0 <= value <= 255:
      raise ValueError('value = %d out of bounds at byte %d' % (value, i))
  return [ctypes.c_ubyte(value) for value in values]


def Uleb128Encode(value: int) -> List[ctypes.c_ubyte]:
  """Encodes the argument int to ULEB128 format.

  Argument:
    value: Unsigned integer.

  Returns:
    List of c_ubyte.
  """
  if value < 0:
    raise ValueError(f'Cannot uleb128 encode negative value ({value}).')

  uleb128_bytes = []
  done = False
  while not done:
    value, lowest_seven_bits = divmod(value, 0x80)
    done = value == 0
    uleb128_bytes.append(lowest_seven_bits | (0x80 if not done else 0x00))
  return EncodeAsCUbytes(*uleb128_bytes)


def EncodeStackPointerUpdate(offset: int) -> List[ctypes.c_ubyte]:
  """Encodes a stack pointer update as arm unwind instructions.

  Argument:
    offset: Offset to apply on stack pointer. Should be in range [-0x204, inf).

  Returns:
    A list of arm unwind instructions in c_ubyte format.
  """
  assert offset % 4 == 0

  abs_offset = abs(offset)
  instruction_code = 0b01000000 if offset < 0 else 0b00000000
  if 0x04 <= abs_offset <= 0x200:
    instructions = [
        # vsp = vsp + (xxxxxx << 2) + 4. Covers range 0x04-0x100 inclusive.
        instruction_code | ((min(abs_offset, 0x100) - 4) >> 2)
    ]
    # For vsp increments of 0x104-0x200 we use 00xxxxxx twice.
    if abs_offset > 0x104:
      instructions.append(instruction_code | ((abs_offset - 0x100 - 4) >> 2))
    try:
      return EncodeAsCUbytes(*instructions)
    except ValueError as e:
      raise RuntimeError('offset = %d produced out of range value' %
                         offset) from e
  else:
    # This only encodes positive sp movement.
    assert offset > 0, offset
    return EncodeAsCUbytes(0b10110010  # vsp = vsp + 0x204 + (uleb128 << 2)
                           ) + Uleb128Encode((offset - 0x204) >> 2)


def EncodePop(registers: Sequence[int]) -> List[ctypes.c_ubyte]:
  """Encodes popping of a sequence of registers as arm unwind instructions.

  Argument:
    registers: Collection of target registers to accept values popped from
      stack. Register value order in the sequence does not matter. Values are
      popped based on register index order.

  Returns:
    A list of arm unwind instructions in c_ubyte format.
  """
  assert all(
      r in range(4, 16)
      for r in registers), f'Can only pop r4 ~ r15. Registers:\n{registers}.'
  assert len(registers) > 0, 'Register sequence cannot be empty.'

  instructions: List[int] = []

  # Check if the pushed registers are continuous set starting from r4 (and
  # ending prior to r12). This scenario has its own encoding.
  pop_lr = 14 in registers
  non_lr_registers = [r for r in registers if r != 14]
  non_lr_registers_continuous_from_r4 = \
    sorted(non_lr_registers) == list(range(4, 4 + len(non_lr_registers)))

  if (pop_lr and 0 < len(non_lr_registers) <= 8
      and non_lr_registers_continuous_from_r4):
    instructions.append(0b10101000
                        | (len(non_lr_registers) - 1)  # Pop r4-r[4+nnn], r14.
                        )
  else:
    register_bits = 0
    for register in registers:
      register_bits |= 1 << register
    register_bits = register_bits >> 4  # Skip r0 ~ r3.
    instructions.extend([
        # Pop up to 12 integer registers under masks {r15-r12}, {r11-r4}.
        0b10000000 | (register_bits >> 8),
        register_bits & 0xff
    ])

  return EncodeAsCUbytes(*instructions)


class UnwindType(enum.Enum):
  """
  The type of unwind action to perform.
  """

  # Use lr as the return address.
  RETURN_TO_LR = 1

  # Increment or decrement the stack pointer and/or pop registers.
  # If both, the increment/decrement occurs first.
  UPDATE_SP_AND_OR_POP_REGISTERS = 2

  # Restore the stack pointer from a register then increment/decrement the stack
  # pointer.
  RESTORE_SP_FROM_REGISTER = 3

  # No action necessary. Used for floating point register pops.
  NO_ACTION = 4


class AddressUnwind(NamedTuple):
  """Record representing unwind information for an address within a function.

  Attributes:
      address_offset: The offset of the address from the start of the function..
      unwind_type: The type of unwind to perform from the address.
      sp_offset: The offset to apply to the stack pointer.
      registers: The registers involved in the unwind.
  """
  address_offset: int
  unwind_type: UnwindType
  sp_offset: int
  registers: Tuple[int, ...]


class FunctionUnwind(NamedTuple):
  """Record representing unwind information for a function.

  Attributes:
      address: The address of the function.
      size: The function size in bytes.
      address_unwind_info: The unwind info at each address in the function.
  """

  address: int
  size: int
  address_unwind: Tuple[AddressUnwind, ...]


def EncodeAddressUnwind(address_unwind: AddressUnwind) -> List[ctypes.c_ubyte]:
  """Encodes an `AddressUnwind` object as arm unwind instructions.

  Argument:
    address_unwind: Record representing unwind information for an address within
      a function.

  Returns:
    A list of arm unwind instructions in c_ubyte format.
  """
  if address_unwind.unwind_type == UnwindType.RETURN_TO_LR:
    return EncodeAsCUbytes(0b10110000  # Finish.
                           )
  if address_unwind.unwind_type == UnwindType.UPDATE_SP_AND_OR_POP_REGISTERS:
    return ((EncodeStackPointerUpdate(address_unwind.sp_offset)
             if address_unwind.sp_offset else []) + (EncodePop(
                 address_unwind.registers) if address_unwind.registers else []))

  if address_unwind.unwind_type == UnwindType.RESTORE_SP_FROM_REGISTER:
    assert len(address_unwind.registers) == 1
    return (EncodeAsCUbytes(0b10010000
                            | address_unwind.registers[0]  # Set vsp = r[nnnn].
                            ) +
            (EncodeStackPointerUpdate(address_unwind.sp_offset)
             if address_unwind.sp_offset else []))

  if address_unwind.unwind_type == UnwindType.NO_ACTION:
    return []

  assert False, 'unknown unwind type'
  return []

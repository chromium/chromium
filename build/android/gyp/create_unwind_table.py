#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a table of unwind information in Android Chrome's bespoke format."""

import re
from typing import Iterable, NamedTuple, TextIO, Tuple

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

#!/usr/bin/env vpython3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''
Embed the address and size of elf sections into the pre-defined symbols.

The embedded values are used for performance optimization by mlock(2)ing the
sections on ChromeOS. See chromeos/ash/components/memory/memory.cc for details.
'''

import argparse
import os
import subprocess
import sys
import shutil

llvm_readelf = os.path.join(
    os.path.dirname(sys.argv[0]), '..', '..', 'third_party', 'llvm-build',
    'Release+Asserts', 'bin', 'llvm-readelf')

TARGET_SECTIONS = {
    '.rodata': {
        'addr': 'kRodataAddr',
        'size': 'kRodataSize'
    },
    '.text.hot': {
        'addr': 'kTextHotAddr',
        'size': 'kTextHotSize'
    },
}


def parse_endianess(objdump_result):
  for line in objdump_result.splitlines():
    line = line.strip()
    if line.startswith('Data:'):
      if '1' in line:
        return 'big'
      if '2' in line:
        return 'little'
  raise ValueError('No endian found')


def assert_elf_type(objdump_result):
  for line in objdump_result.splitlines():
    line = line.strip()
    if line.startswith('Class:'):
      if 'ELF64' in line or 'ELF32' in line:
        return
      raise ValueError('Class is not ELF64 nor ELF32: ' + line)
  raise ValueError('No class found')


def parse_section_info(objdump_result, section_name):
  for line in objdump_result.splitlines():
    row = line.strip().split()
    if len(row) < 2:
      continue
    if row[1] == section_name:
      # 3: Address, 4: Offset, 5: Size
      return (int(row[3], base=16), int(row[4], base=16), int(row[5], base=16))
  return (0, 0, 0)


def create_symbol_map(binary_input, objdump_result):
  (rodata_section_addr, rodata_section_offset,
   rodata_section_size) = parse_section_info(objdump_result, '.rodata')

  command = [llvm_readelf, '--symbols', binary_input]
  with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as process:

    variable_names = [
        var for maps in TARGET_SECTIONS.values() for var in maps.values()
    ]
    result = {}
    while len(result) < len(variable_names):
      line = process.stdout.readline()
      if not line:
        break
      for var in variable_names:
        if var in line:
          row = line.strip().split()
          if row[2] == '8':
            size = 8
          elif row[2] == '4':
            size = 4
          else:
            raise ValueError('variable size is not 8 or 4: ' + line)
          addr = int(row[1], base=16)
          rodata_section_end_addr = rodata_section_addr + rodata_section_size
          if addr < rodata_section_addr or addr >= rodata_section_end_addr:
            raise ValueError(var + ' is not in .rodata section')
          offset = addr - rodata_section_addr + rodata_section_offset
          result[var] = (offset, size)

  return result


def overwrite_variable(file, symbol_map, endianess, section_name, var_type,
                       value):
  (var_offset, var_size) = symbol_map[TARGET_SECTIONS[section_name][var_type]]
  file.seek(var_offset)
  if file.write(
      value.to_bytes(length=var_size, byteorder=endianess,
                     signed=False)) != var_size:
    raise ValueError('failed to write value to file')


def main():
  argparser = argparse.ArgumentParser(
      description='embed sections informataion into binary.')

  argparser.add_argument('--binary-input', help='exe file path.')
  argparser.add_argument('--binary-output', help='embedded file path.')
  args = argparser.parse_args()

  objdump_result = subprocess.run([llvm_readelf, '-e', args.binary_input],
                                  stdout=subprocess.PIPE,
                                  check=True,
                                  text=True).stdout

  assert_elf_type(objdump_result)

  symbol_map = create_symbol_map(args.binary_input, objdump_result)
  if len(symbol_map) != len(set(symbol_map.values())):
    raise ValueError(f'symbol_map overlaps: {symbol_map}')

  endianess = parse_endianess(objdump_result)

  shutil.copyfile(args.binary_input, args.binary_output)

  with open(args.binary_output, 'r+b') as file:
    for section_name in TARGET_SECTIONS:
      (addr, _, size) = parse_section_info(objdump_result, section_name)
      overwrite_variable(file, symbol_map, endianess, section_name, 'addr',
                         addr)
      overwrite_variable(file, symbol_map, endianess, section_name, 'size',
                         size)

  objdump_result_after = subprocess.run(
      [llvm_readelf, '-e', args.binary_output],
      stdout=subprocess.PIPE,
      check=True,
      text=True).stdout
  if objdump_result_after != objdump_result:
    raise ValueError('realelf result has changed')

  return 0


if __name__ == '__main__':
  sys.exit(main())

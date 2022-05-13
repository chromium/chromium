#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps ml.exe or ml64.exe and postprocesses the output to be deterministic.
Sets timestamp in .obj file to 0, hence incompatible with link.exe /incremental.

Use by prefixing the ml(64).exe invocation with this script:
    python ml.py ml.exe [args...]"""

import array
import collections
import struct
import subprocess
import sys


class Struct(object):
  """A thin wrapper around the struct module that returns a namedtuple"""
  def __init__(self, name, *args):
    """Pass the name of the return type, and then an interleaved list of
    format strings as used by the struct module and of field names."""
    self.fmt = '<' + ''.join(args[0::2])
    self.type = collections.namedtuple(name, args[1::2])

  def pack_into(self, buffer, offset, data):
    return struct.pack_into(self.fmt, buffer, offset, *data)

  def unpack_from(self, buffer, offset=0):
    return self.type(*struct.unpack_from(self.fmt, buffer, offset))

  def size(self):
    return struct.calcsize(self.fmt)


def Subtract(nt, **kwargs):
  """Subtract(nt, f=2) returns a new namedtuple with 2 subtracted from nt.f"""
  return nt._replace(**{k: getattr(nt, k) - v for k, v in kwargs.items()})


def MakeDeterministic(objdata):
  # Takes data produced by ml(64).exe (without any special flags) and
  # 1. Sets the timestamp to 0
  # 2. Strips the .debug$S section (which contains an unwanted absolute path)

  # This makes several assumptions about ml's output:
  # - Section data is in the same order as the corresponding section headers:
  #   section headers preceding the .debug$S section header have their data
  #   preceding the .debug$S section data; likewise for section headers
  #   following the .debug$S section.
  # - The .debug$S section contains only the absolute path to the obj file and
  #   nothing else, in particular there's only a single entry in the symbol
  #   table referring to the .debug$S section.
  # - There are no COFF line number entries.
  # - There's no IMAGE_SYM_CLASS_CLR_TOKEN symbol.
  # These seem to hold in practice; if they stop holding this script needs to
  # become smarter.

  objdata = array.array('b', objdata)  # Writable, e.g. via struct.pack_into.

  # Read coff header.
  COFFHEADER = Struct('COFFHEADER',
                      'H', 'Machine',
                      'H', 'NumberOfSections',
                      'I', 'TimeDateStamp',
                      'I', 'PointerToSymbolTable',
                      'I', 'NumberOfSymbols',

                      'H', 'SizeOfOptionalHeader',
                      'H', 'Characteristics')
  coff_header = COFFHEADER.unpack_from(objdata)
  assert coff_header.SizeOfOptionalHeader == 0  # Only set for binaries.

  # Read section headers following coff header.
  SECTIONHEADER = Struct('SECTIONHEADER',
                         '8s', 'Name',
                         'I', 'VirtualSize',
                         'I', 'VirtualAddress',

                         'I', 'SizeOfRawData',
                         'I', 'PointerToRawData',
                         'I', 'PointerToRelocations',
                         'I', 'PointerToLineNumbers',

                         'H', 'NumberOfRelocations',
                         'H', 'NumberOfLineNumbers',
                         'I', 'Characteristics')
  section_headers = []
  debug_section_index = -1
  for i in range(0, coff_header.NumberOfSections):
    section_header = SECTIONHEADER.unpack_from(
        objdata, offset=COFFHEADER.size() + i * SECTIONHEADER.size())
    assert not section_header[0].startswith(b'/')  # Support short names only.
    section_headers.append(section_header)

    if section_header.Name == b'.debug$S':
      assert debug_section_index == -1
      debug_section_index = i
  assert debug_section_index != -1

  data_start = COFFHEADER.size() + len(section_headers) * SECTIONHEADER.size()

  # Verify the .debug$S section looks like we expect.
  assert section_headers[debug_section_index].Name == b'.debug$S'
  assert section_headers[debug_section_index].VirtualSize == 0
  assert section_headers[debug_section_index].VirtualAddress == 0
  debug_size = section_headers[debug_section_index].SizeOfRawData
  debug_offset = section_headers[debug_section_index].PointerToRawData
  assert section_headers[debug_section_index].PointerToRelocations == 0
  assert section_headers[debug_section_index].PointerToLineNumbers == 0
  assert section_headers[debug_section_index].NumberOfRelocations == 0
  assert section_headers[debug_section_index].NumberOfLineNumbers == 0

  # Make sure sections in front of .debug$S have their data preceding it.
  for header in section_headers[:debug_section_index]:
    assert header.PointerToRawData < debug_offset
    assert header.PointerToRelocations < debug_offset
    assert header.PointerToLineNumbers < debug_offset

  # Make sure sections after of .debug$S have their data following it.
  for header in section_headers[debug_section_index + 1:]:
    # Make sure the .debug$S data is at the very end of section data:
    assert header.PointerToRawData > debug_offset
    assert header.PointerToRelocations == 0
    assert header.PointerToLineNumbers == 0

  # Make sure the first non-empty section's data starts right after the section
  # headers.
  for section_header in section_headers:
    if section_header.PointerToRawData == 0:
      assert section_header.PointerToRelocations == 0
      assert section_header.PointerToLineNumbers == 0
      continue
    assert section_header.PointerToRawData == data_start
    break

  # Make sure the symbol table (and hence, string table) appear after the last
  # section:
  assert (coff_header.PointerToSymbolTable >=
      section_headers[-1].PointerToRawData + section_headers[-1].SizeOfRawData)

  # The symbol table contains a symbol for the no-longer-present .debug$S
  # section. If we leave it there, lld-link will complain:
  #
  #    lld-link: error: .debug$S should not refer to non-existent section 5
  #
  # so we need to remove that symbol table entry as well. This shifts symbol
  # entries around and we need to update symbol table indices in:
  # - relocations
  # - line number records (never present)
  # - one aux symbol entry (IMAGE_SYM_CLASS_CLR_TOKEN; not present in ml output)
  SYM = Struct('SYM',
               '8s', 'Name',
               'I', 'Value',
               'h', 'SectionNumber',  # Note: Signed!
               'H', 'Type',

               'B', 'StorageClass',
               'B', 'NumberOfAuxSymbols')
  i = 0
  debug_sym = -1
  while i < coff_header.NumberOfSymbols:
    sym_offset = coff_header.PointerToSymbolTable + i * SYM.size()
    sym = SYM.unpack_from(objdata, sym_offset)

    # 107 is IMAGE_SYM_CLASS_CLR_TOKEN, which has aux entry "CLR Token
    # Definition", which contains a symbol index. Check it's never present.
    assert sym.StorageClass != 107

    # Note: sym.SectionNumber is 1-based, debug_section_index is 0-based.
    if sym.SectionNumber - 1 == debug_section_index:
      assert debug_sym == -1, 'more than one .debug$S symbol found'
      debug_sym = i
      # Make sure the .debug$S symbol looks like we expect.
      # In particular, it should have exactly one aux symbol.
      assert sym.Name == b'.debug$S'
      assert sym.Value == 0
      assert sym.Type == 0
      assert sym.StorageClass == 3
      assert sym.NumberOfAuxSymbols == 1
    elif sym.SectionNumber > debug_section_index:
      sym = Subtract(sym, SectionNumber=1)
      SYM.pack_into(objdata, sym_offset, sym)
    i += 1 + sym.NumberOfAuxSymbols
  assert debug_sym != -1, '.debug$S symbol not found'

  # Note: Usually the .debug$S section is the last, but for files saying
  # `includelib foo.lib`, like safe_terminate_process.asm in 32-bit builds,
  # this isn't true: .drectve is after .debug$S.

  # Update symbol table indices in relocations.
  # There are a few processor types that have one or two relocation types
  # where SymbolTableIndex has a different meaning, but not for x86.
  REL = Struct('REL',
               'I', 'VirtualAddress',
               'I', 'SymbolTableIndex',
               'H', 'Type')
  for header in section_headers[0:debug_section_index]:
    for j in range(0, header.NumberOfRelocations):
      rel_offset = header.PointerToRelocations + j * REL.size()
      rel = REL.unpack_from(objdata, rel_offset)
      assert rel.SymbolTableIndex != debug_sym
      if rel.SymbolTableIndex > debug_sym:
        rel = Subtract(rel, SymbolTableIndex=2)
        REL.pack_into(objdata, rel_offset, rel)

  # Update symbol table indices in line numbers -- just check they don't exist.
  for header in section_headers:
    assert header.NumberOfLineNumbers == 0

  # Now that all indices are updated, remove the symbol table entry referring to
  # .debug$S and its aux entry.
  del objdata[coff_header.PointerToSymbolTable + debug_sym * SYM.size():
              coff_header.PointerToSymbolTable + (debug_sym + 2) * SYM.size()]

  # Now we know that it's safe to write out the input data, with just the
  # timestamp overwritten to 0, the last section header cut out (and the
  # offsets of all other section headers decremented by the size of that
  # one section header), and the last section's data cut out. The symbol
  # table offset needs to be reduced by one section header and the size of
  # the missing section.
  # (The COFF spec only requires on-disk sections to be aligned in image files,
  # for obj files it's not required. If that wasn't the case, deleting slices
  # if data would not generally be safe.)

  # Update section offsets and remove .debug$S section data.
  for i in range(0, debug_section_index):
    header = section_headers[i]
    if header.SizeOfRawData:
      header = Subtract(header, PointerToRawData=SECTIONHEADER.size())
    if header.NumberOfRelocations:
      header = Subtract(header, PointerToRelocations=SECTIONHEADER.size())
    if header.NumberOfLineNumbers:
      header = Subtract(header, PointerToLineNumbers=SECTIONHEADER.size())
    SECTIONHEADER.pack_into(
        objdata, COFFHEADER.size() + i * SECTIONHEADER.size(), header)
  for i in range(debug_section_index + 1, len(section_headers)):
    header = section_headers[i]
    shift = SECTIONHEADER.size() + debug_size
    if header.SizeOfRawData:
      header = Subtract(header, PointerToRawData=shift)
    if header.NumberOfRelocations:
      header = Subtract(header, PointerToRelocations=shift)
    if header.NumberOfLineNumbers:
      header = Subtract(header, PointerToLineNumbers=shift)
    SECTIONHEADER.pack_into(
        objdata, COFFHEADER.size() + i * SECTIONHEADER.size(), header)

  del objdata[debug_offset:debug_offset + debug_size]

  # Finally, remove .debug$S section header and update coff header.
  coff_header = coff_header._replace(TimeDateStamp=0)
  coff_header = Subtract(coff_header,
                         NumberOfSections=1,
                         PointerToSymbolTable=SECTIONHEADER.size() + debug_size,
                         NumberOfSymbols=2)
  COFFHEADER.pack_into(objdata, 0, coff_header)

  del objdata[
      COFFHEADER.size() + debug_section_index * SECTIONHEADER.size():
      COFFHEADER.size() + (debug_section_index + 1) * SECTIONHEADER.size()]

  # All done!
  if sys.version_info.major == 2:
    return objdata.tostring()
  else:
    return objdata.tobytes()


def main():
  ml_result = subprocess.call(sys.argv[1:])
  if ml_result != 0:
    return ml_result

  objfile = None
  for i in range(1, len(sys.argv)):
    if sys.argv[i].startswith('/Fo'):
      objfile = sys.argv[i][len('/Fo'):]
  assert objfile, 'failed to find ml output'

  with open(objfile, 'rb') as f:
    objdata = f.read()
  objdata = MakeDeterministic(objdata)
  with open(objfile, 'wb') as f:
    f.write(objdata)


if __name__ == '__main__':
  sys.exit(main())

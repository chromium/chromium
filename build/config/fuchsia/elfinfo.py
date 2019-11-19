# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses ELF information without relying external tools.

This file was originally copied and adapted from:
https://fuchsia.googlesource.com/fuchsia/+/827f9fe/build/images/elfinfo.py
"""

from contextlib import contextmanager
from collections import namedtuple
import mmap
import os
import struct
import uuid

# Standard ELF constants.
ELFMAG = '\x7fELF'
EI_CLASS = 4
ELFCLASS32 = 1
ELFCLASS64 = 2
EI_DATA = 5
ELFDATA2LSB = 1
ELFDATA2MSB = 2
EM_386 = 3
EM_ARM = 40
EM_X86_64 = 62
EM_AARCH64 = 183
PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_NOTE = 4
DT_NEEDED = 1
DT_STRTAB = 5
DT_SONAME = 14
NT_GNU_BUILD_ID = 3
SHT_SYMTAB = 2


class elf_note(namedtuple('elf_note', [
    'name',
    'type',
    'desc',
])):

  # An ELF note is identified by (name_string, type_integer).
  def ident(self):
    return (self.name, self.type)

  def is_build_id(self):
    return self.ident() == ('GNU\0', NT_GNU_BUILD_ID)

  def build_id_hex(self):
    if self.is_build_id():
      return ''.join(('%02x' % ord(byte)) for byte in self.desc)
    return None

  def __repr__(self):
    return ('elf_note(%r, %#x, <%d bytes>)' % (self.name, self.type,
                                               len(self.desc)))


def gen_elf():
  # { 'Struct1': (ELFCLASS32 fields, ELFCLASS64 fields),
  #   'Struct2': fields_same_for_both, ... }
  elf_types = {
      'Ehdr': ([
          ('e_ident', '16s'),
          ('e_type', 'H'),
          ('e_machine', 'H'),
          ('e_version', 'I'),
          ('e_entry', 'I'),
          ('e_phoff', 'I'),
          ('e_shoff', 'I'),
          ('e_flags', 'I'),
          ('e_ehsize', 'H'),
          ('e_phentsize', 'H'),
          ('e_phnum', 'H'),
          ('e_shentsize', 'H'),
          ('e_shnum', 'H'),
          ('e_shstrndx', 'H'),
      ], [
          ('e_ident', '16s'),
          ('e_type', 'H'),
          ('e_machine', 'H'),
          ('e_version', 'I'),
          ('e_entry', 'Q'),
          ('e_phoff', 'Q'),
          ('e_shoff', 'Q'),
          ('e_flags', 'I'),
          ('e_ehsize', 'H'),
          ('e_phentsize', 'H'),
          ('e_phnum', 'H'),
          ('e_shentsize', 'H'),
          ('e_shnum', 'H'),
          ('e_shstrndx', 'H'),
      ]),
      'Phdr': ([
          ('p_type', 'I'),
          ('p_offset', 'I'),
          ('p_vaddr', 'I'),
          ('p_paddr', 'I'),
          ('p_filesz', 'I'),
          ('p_memsz', 'I'),
          ('p_flags', 'I'),
          ('p_align', 'I'),
      ], [
          ('p_type', 'I'),
          ('p_flags', 'I'),
          ('p_offset', 'Q'),
          ('p_vaddr', 'Q'),
          ('p_paddr', 'Q'),
          ('p_filesz', 'Q'),
          ('p_memsz', 'Q'),
          ('p_align', 'Q'),
      ]),
      'Shdr': ([
          ('sh_name', 'L'),
          ('sh_type', 'L'),
          ('sh_flags', 'L'),
          ('sh_addr', 'L'),
          ('sh_offset', 'L'),
          ('sh_size', 'L'),
          ('sh_link', 'L'),
          ('sh_info', 'L'),
          ('sh_addralign', 'L'),
          ('sh_entsize', 'L'),
      ], [
          ('sh_name', 'L'),
          ('sh_type', 'L'),
          ('sh_flags', 'Q'),
          ('sh_addr', 'Q'),
          ('sh_offset', 'Q'),
          ('sh_size', 'Q'),
          ('sh_link', 'L'),
          ('sh_info', 'L'),
          ('sh_addralign', 'Q'),
          ('sh_entsize', 'Q'),
      ]),
      'Dyn': ([
          ('d_tag', 'i'),
          ('d_val', 'I'),
      ], [
          ('d_tag', 'q'),
          ('d_val', 'Q'),
      ]),
      'Nhdr': [
          ('n_namesz', 'I'),
          ('n_descsz', 'I'),
          ('n_type', 'I'),
      ],
      'dwarf2_line_header': [
          ('unit_length', 'L'),
          ('version', 'H'),
          ('header_length', 'L'),
          ('minimum_instruction_length', 'B'),
          ('default_is_stmt', 'B'),
          ('line_base', 'b'),
          ('line_range', 'B'),
          ('opcode_base', 'B'),
      ],
      'dwarf4_line_header': [
          ('unit_length', 'L'),
          ('version', 'H'),
          ('header_length', 'L'),
          ('minimum_instruction_length', 'B'),
          ('maximum_operations_per_instruction', 'B'),
          ('default_is_stmt', 'B'),
          ('line_base', 'b'),
          ('line_range', 'b'),
          ('opcode_base', 'B'),
      ],
  }

  # There is an accessor for each struct, e.g. Ehdr.
  # Ehdr.read is a function like Struct.unpack_from.
  # Ehdr.size is the size of the struct.
  elf_accessor = namedtuple('elf_accessor', ['size', 'read', 'write', 'pack'])

  # All the accessors for a format (class, byte-order) form one elf,
  # e.g. use elf.Ehdr and elf.Phdr.
  elf = namedtuple('elf', elf_types.keys())

  def gen_accessors(is64, struct_byte_order):

    def make_accessor(type, decoder):
      return elf_accessor(
          size=decoder.size,
          read=lambda buffer, offset=0: type._make(
              decoder.unpack_from(buffer, offset)),
          write=lambda buffer, offset, x: decoder.pack_into(
              buffer, offset, *x),
          pack=lambda x: decoder.pack(*x))

    for name, fields in elf_types.iteritems():
      if isinstance(fields, tuple):
        fields = fields[1 if is64 else 0]
      type = namedtuple(name, [field_name for field_name, fmt in fields])
      decoder = struct.Struct(struct_byte_order + ''.join(
          fmt for field_name, fmt in fields))
      yield make_accessor(type, decoder)

  for elfclass, is64 in [(ELFCLASS32, False), (ELFCLASS64, True)]:
    for elf_bo, struct_bo in [(ELFDATA2LSB, '<'), (ELFDATA2MSB, '>')]:
      yield ((chr(elfclass), chr(elf_bo)), elf(*gen_accessors(is64, struct_bo)))


# e.g. ELF[file[EI_CLASS], file[EI_DATA]].Ehdr.read(file).e_phnum
ELF = dict(gen_elf())


def get_elf_accessor(file):
  # If it looks like an ELF file, whip out the decoder ring.
  if file[:len(ELFMAG)] == ELFMAG:
    return ELF[file[EI_CLASS], file[EI_DATA]]
  return None


def gen_phdrs(file, elf, ehdr):
  for pos in xrange(0, ehdr.e_phnum * elf.Phdr.size, elf.Phdr.size):
    yield elf.Phdr.read(file, ehdr.e_phoff + pos)


def gen_shdrs(file, elf, ehdr):
  for pos in xrange(0, ehdr.e_shnum * elf.Shdr.size, elf.Shdr.size):
    yield elf.Shdr.read(file, ehdr.e_shoff + pos)


cpu = namedtuple(
    'cpu',
    [
        'e_machine',  # ELF e_machine int
        'llvm',  # LLVM triple CPU component
        'gn',  # GN target_cpu
    ])

ELF_MACHINE_TO_CPU = {
    elf: cpu(elf, llvm, gn) for elf, llvm, gn in [
        (EM_386, 'i386', 'x86'),
        (EM_ARM, 'arm', 'arm'),
        (EM_X86_64, 'x86_64', 'x64'),
        (EM_AARCH64, 'aarch64', 'arm64'),
    ]
}


@contextmanager
def mmapper(filename):
  """A context manager that yields (fd, file_contents) given a file name.
This ensures that the mmap and file objects are closed at the end of the
'with' statement."""
  fileobj = open(filename, 'rb')
  fd = fileobj.fileno()
  if os.fstat(fd).st_size == 0:
    # mmap can't handle empty files.
    try:
      yield fd, ''
    finally:
      fileobj.close()
  else:
    mmapobj = mmap.mmap(fd, 0, access=mmap.ACCESS_READ)
    try:
      yield fd, mmapobj
    finally:
      mmapobj.close()
      fileobj.close()


elf_info = namedtuple(
    'elf_info',
    [
        'filename',
        'cpu',  # cpu tuple
        'notes',  # list of (ident, desc): selected notes
        'build_id',  # string: lowercase hex
        'stripped',  # bool: Has no symbols or .debug_* sections
        'interp',  # string or None: PT_INTERP (without \0)
        'soname',  # string or None: DT_SONAME
        'needed',  # list of strings: DT_NEEDED
    ])


def get_elf_info(filename, match_notes=False):
  file = None
  elf = None
  ehdr = None
  phdrs = None

  # Yields an elf_note for each note in any PT_NOTE segment.
  def gen_notes():

    def round_up_to(size):
      return ((size + 3) / 4) * 4

    for phdr in phdrs:
      if phdr.p_type == PT_NOTE:
        pos = phdr.p_offset
        while pos < phdr.p_offset + phdr.p_filesz:
          nhdr = elf.Nhdr.read(file, pos)
          pos += elf.Nhdr.size
          name = file[pos:pos + nhdr.n_namesz]
          pos += round_up_to(nhdr.n_namesz)
          desc = file[pos:pos + nhdr.n_descsz]
          pos += round_up_to(nhdr.n_descsz)
          yield elf_note(name, nhdr.n_type, desc)

  def gen_sections():
    shdrs = list(gen_shdrs(file, elf, ehdr))
    if not shdrs:
      return
    strtab_shdr = shdrs[ehdr.e_shstrndx]
    for shdr, i in zip(shdrs, xrange(len(shdrs))):
      if i == 0:
        continue
      assert shdr.sh_name < strtab_shdr.sh_size, (
          "%s: invalid sh_name" % filename)
      yield (shdr, extract_C_string(strtab_shdr.sh_offset + shdr.sh_name))

  # Generates '\0'-terminated strings starting at the given offset,
  # until an empty string.
  def gen_strings(start):
    while True:
      end = file.find('\0', start)
      assert end >= start, (
          "%s: Unterminated string at %#x" % (filename, start))
      if start == end:
        break
      yield file[start:end]
      start = end + 1

  def extract_C_string(start):
    for string in gen_strings(start):
      return string
    return ''

  # Returns a string of hex digits (or None).
  def get_build_id():
    build_id = None
    for note in gen_notes():
      # Note that the last build_id note needs to be used due to TO-442.
      possible_build_id = note.build_id_hex()
      if possible_build_id:
        build_id = possible_build_id
    return build_id

  # Returns a list of elf_note objects.
  def get_matching_notes():
    if isinstance(match_notes, bool):
      if match_notes:
        return list(gen_notes())
      else:
        return []
    # If not a bool, it's an iterable of ident pairs.
    return [note for note in gen_notes() if note.ident() in match_notes]

  # Returns a string (without trailing '\0'), or None.
  def get_interp():
    # PT_INTERP points directly to a string in the file.
    for interp in (phdr for phdr in phdrs if phdr.p_type == PT_INTERP):
      interp = file[interp.p_offset:interp.p_offset + interp.p_filesz]
      if interp[-1:] == '\0':
        interp = interp[:-1]
      return interp
    return None

  # Returns a set of strings.
  def get_soname_and_needed():
    # Each DT_NEEDED or DT_SONAME points to a string in the .dynstr table.
    def GenDTStrings(tag):
      return (extract_C_string(strtab_offset + dt.d_val)
              for dt in dyn
              if dt.d_tag == tag)

    # PT_DYNAMIC points to the list of ElfNN_Dyn tags.
    for dynamic in (phdr for phdr in phdrs if phdr.p_type == PT_DYNAMIC):
      dyn = [
          elf.Dyn.read(file, dynamic.p_offset + dyn_offset)
          for dyn_offset in xrange(0, dynamic.p_filesz, elf.Dyn.size)
      ]

      # DT_STRTAB points to the string table's vaddr (.dynstr).
      [strtab_vaddr] = [dt.d_val for dt in dyn if dt.d_tag == DT_STRTAB]

      # Find the PT_LOAD containing the vaddr to compute the file offset.
      [strtab_offset] = [
          strtab_vaddr - phdr.p_vaddr + phdr.p_offset
          for phdr in phdrs
          if (phdr.p_type == PT_LOAD and phdr.p_vaddr <= strtab_vaddr and
              strtab_vaddr - phdr.p_vaddr < phdr.p_filesz)
      ]

      soname = None
      for soname in GenDTStrings(DT_SONAME):
        break

      return soname, set(GenDTStrings(DT_NEEDED))
    return None, set()

  def get_stripped():
    return all(shdr.sh_type != SHT_SYMTAB and not name.startswith('.debug_')
               for shdr, name in gen_sections())

  def get_cpu():
    return ELF_MACHINE_TO_CPU.get(ehdr.e_machine)

  # Map in the whole file's contents and use it as a string.
  with mmapper(filename) as mapped:
    fd, file = mapped
    elf = get_elf_accessor(file)
    if elf is not None:
      # ELF header leads to program headers.
      ehdr = elf.Ehdr.read(file)
      assert ehdr.e_phentsize == elf.Phdr.size, (
          "%s: invalid e_phentsize" % filename)
      phdrs = list(gen_phdrs(file, elf, ehdr))
      return elf_info(filename, get_cpu(), get_matching_notes(), get_build_id(),
                      get_stripped(), get_interp(), *get_soname_and_needed())

  return None


# Module public API.
__all__ = ['cpu', 'elf_info', 'elf_note', 'get_elf_accessor', 'get_elf_info']

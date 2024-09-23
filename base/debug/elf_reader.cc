// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/elf_reader.h"

#include <arpa/inet.h>
#include <elf.h>
#include <string.h>

#include <optional>
#include <string_view>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/safe_sprintf.h"
#include "build/build_config.h"

// NOTE: This code may be used in crash handling code, so the implementation
// must avoid dynamic memory allocation or using data structures which rely on
// dynamic allocation.

namespace base {
namespace debug {
namespace {

// See https://refspecs.linuxbase.org/elf/elf.pdf for the ELF specification.

#if __SIZEOF_POINTER__ == 4
using Ehdr = Elf32_Ehdr;
using Dyn = Elf32_Dyn;
using Half = Elf32_Half;
using Nhdr = Elf32_Nhdr;
using Word = Elf32_Word;
using Xword = Elf32_Word;
#else
using Ehdr = Elf64_Ehdr;
using Dyn = Elf64_Dyn;
using Half = Elf64_Half;
using Nhdr = Elf64_Nhdr;
using Word = Elf64_Word;
using Xword = Elf64_Xword;
#endif

constexpr char kGnuNoteName[] = "GNU";

// Returns a pointer to the header of the ELF binary mapped into memory, or a
// null pointer if the header is invalid. Here and below |elf_mapped_base| is a
// pointer to the start of the ELF image.
const Ehdr* GetElfHeader(const void* elf_mapped_base) {
  if (strncmp(reinterpret_cast<const char*>(elf_mapped_base), ELFMAG,
              SELFMAG) != 0)
    return nullptr;

  return reinterpret_cast<const Ehdr*>(elf_mapped_base);
}

}  // namespace

size_t ReadElfBuildId(const void* elf_mapped_base,
                      bool uppercase,
                      ElfBuildIdBuffer build_id) {
  // NOTE: Function should use async signal safe calls only.

  const Ehdr* elf_header = GetElfHeader(elf_mapped_base);
  if (!elf_header)
    return 0;

  const size_t relocation_offset = GetRelocationOffset(elf_mapped_base);
  for (const Phdr& header : GetElfProgramHeaders(elf_mapped_base)) {
    if (header.p_type != PT_NOTE)
      continue;

    // Look for a NT_GNU_BUILD_ID note with name == "GNU".
    const char* current_section =
        reinterpret_cast<const char*>(header.p_vaddr + relocation_offset);
    const char* section_end = current_section + header.p_memsz;
    const Nhdr* current_note = nullptr;
    bool found = false;
    while (current_section < section_end) {
      current_note = reinterpret_cast<const Nhdr*>(current_section);
      if (current_note->n_type == NT_GNU_BUILD_ID) {
        std::string_view note_name(current_section + sizeof(Nhdr),
                                   current_note->n_namesz);
        // Explicit constructor is used to include the '\0' character.
        if (note_name == std::string_view(kGnuNoteName, sizeof(kGnuNoteName))) {
          found = true;
          break;
        }
      }

      size_t section_size = bits::AlignUp(current_note->n_namesz, 4u) +
                            bits::AlignUp(current_note->n_descsz, 4u) +
                            sizeof(Nhdr);
      if (section_size > static_cast<size_t>(section_end - current_section))
        return 0;
      current_section += section_size;
    }

    if (!found)
      continue;

    // Validate that the serialized build ID will fit inside |build_id|.
    size_t note_size = current_note->n_descsz;
    if ((note_size * 2) > kMaxBuildIdStringLength)
      continue;

    // Write out the build ID as a null-terminated hex string.
    const uint8_t* build_id_raw =
        reinterpret_cast<const uint8_t*>(current_note) + sizeof(Nhdr) +
        bits::AlignUp(current_note->n_namesz, 4u);
    size_t i = 0;
    for (i = 0; i < current_note->n_descsz; ++i) {
      strings::SafeSNPrintf(&build_id[i * 2], 3, (uppercase ? "%02X" : "%02x"),
                            build_id_raw[i]);
    }
    build_id[i * 2] = '\0';

    // Return the length of the string.
    return i * 2;
  }

  return 0;
}

std::optional<std::string_view> ReadElfLibraryName(
    const void* elf_mapped_base) {
  // NOTE: Function should use async signal safe calls only.

  const Ehdr* elf_header = GetElfHeader(elf_mapped_base);
  if (!elf_header)
    return {};

  const size_t relocation_offset = GetRelocationOffset(elf_mapped_base);
  for (const Phdr& header : GetElfProgramHeaders(elf_mapped_base)) {
    if (header.p_type != PT_DYNAMIC)
      continue;

    // Read through the ELF dynamic sections to find the string table and
    // SONAME offsets, which are used to compute the offset of the library
    // name string.
    const Dyn* dynamic_start =
        reinterpret_cast<const Dyn*>(header.p_vaddr + relocation_offset);
    const Dyn* dynamic_end = reinterpret_cast<const Dyn*>(
        header.p_vaddr + relocation_offset + header.p_memsz);
    Xword soname_strtab_offset = 0;
    const char* strtab_addr = 0;
    for (const Dyn* dynamic_iter = dynamic_start; dynamic_iter < dynamic_end;
         ++dynamic_iter) {
      if (dynamic_iter->d_tag == DT_STRTAB) {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
        // Fuchsia and Android do not relocate the symtab pointer on ELF load.
        strtab_addr = static_cast<size_t>(dynamic_iter->d_un.d_ptr) +
                      reinterpret_cast<const char*>(relocation_offset);
#else
        strtab_addr = reinterpret_cast<const char*>(dynamic_iter->d_un.d_ptr);
#endif
      } else if (dynamic_iter->d_tag == DT_SONAME) {
        // The Android NDK wrongly defines `d_val` as an Elf32_Sword for 32 bits
        // and thus needs this cast.
        soname_strtab_offset = static_cast<Xword>(dynamic_iter->d_un.d_val);
      }
    }
    if (soname_strtab_offset && strtab_addr)
      return std::string_view(strtab_addr + soname_strtab_offset);
  }

  return std::nullopt;
}

span<const Phdr> GetElfProgramHeaders(const void* elf_mapped_base) {
  // NOTE: Function should use async signal safe calls only.

  const Ehdr* elf_header = GetElfHeader(elf_mapped_base);
  if (!elf_header)
    return {};

  const char* phdr_start =
      reinterpret_cast<const char*>(elf_header) + elf_header->e_phoff;
  return span<const Phdr>(reinterpret_cast<const Phdr*>(phdr_start),
                          elf_header->e_phnum);
}

// Returns the offset to add to virtual addresses in the image to compute the
// mapped virtual address.
size_t GetRelocationOffset(const void* elf_mapped_base) {
  span<const Phdr> headers = GetElfProgramHeaders(elf_mapped_base);
  for (const Phdr& header : headers) {
    if (header.p_type == PT_LOAD) {
      // |elf_mapped_base| + |header.p_offset| is the mapped address of this
      // segment. |header.p_vaddr| is the specified virtual address within the
      // ELF image.
      const char* const mapped_address =
          reinterpret_cast<const char*>(elf_mapped_base) + header.p_offset;
      return reinterpret_cast<uintptr_t>(mapped_address) - header.p_vaddr;
    }
  }

  // Assume the virtual addresses in the image start at 0, so the offset is
  // from 0 to the actual mapped base address.
  return static_cast<size_t>(reinterpret_cast<uintptr_t>(elf_mapped_base) -
                             reinterpret_cast<uintptr_t>(nullptr));
}

}  // namespace debug
}  // namespace base

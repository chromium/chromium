// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader.h"

#include <arpa/inet.h>
#include <elf.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

namespace base::debug {
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

// Release builds use SHA1 (20 bytes), but other linkers may use other methods.
// Support up to 32 bytes (for blake3 & SHA256).
constexpr size_t kMaxBuildIdByteLength = 32;

// Returns a pointer to the header of the ELF binary mapped into memory, or a
// null pointer if the header is invalid. Here and below |elf_mapped_base| is a
// pointer to the start of the ELF image.
const Ehdr* GetElfHeader(const void* elf_mapped_base) {
  const Ehdr* elf_header = reinterpret_cast<const Ehdr*>(elf_mapped_base);
  if (span(elf_header->e_ident).first<SELFMAG>() !=
      byte_span_from_cstring(ELFMAG)) {
    return nullptr;
  }

  return elf_header;
}

// Returns the offset to add to virtual addresses in the image mapped at
// `elf_mapped_base` to compute the mapped virtual address, given the image's
// `program_headers`.
size_t ComputeRelocationOffset(const void* elf_mapped_base,
                               span<const Phdr> program_headers) {
  for (const Phdr& header : program_headers) {
    if (header.p_type == PT_LOAD) {
      // `elf_mapped_base` + `header.p_offset` is the mapped address of this
      // segment. `header.p_vaddr` is the specified virtual address within the
      // ELF image.
      const uintptr_t mapped_address =
          reinterpret_cast<uintptr_t>(elf_mapped_base) + header.p_offset;
      return mapped_address - header.p_vaddr;
    }
  }

  // Assume the virtual addresses in the image start at 0, so the offset is
  // from 0 to the actual mapped base address.
  return static_cast<size_t>(reinterpret_cast<uintptr_t>(elf_mapped_base) -
                             reinterpret_cast<uintptr_t>(nullptr));
}

}  // namespace

std::string ReadElfBuildId(const void* elf_mapped_base, bool uppercase) {
  // GetElfProgramHeaders() validates the ELF header and returns an empty span
  // if it is invalid.
  // SAFETY: Required from caller, see UNSAFE_BUFFER_USAGE in header.
  const span<const Phdr> program_headers =
      UNSAFE_BUFFERS(GetElfProgramHeaders(elf_mapped_base));
  const size_t relocation_offset =
      ComputeRelocationOffset(elf_mapped_base, program_headers);
  for (const Phdr& header : program_headers) {
    if (header.p_type != PT_NOTE) {
      continue;
    }

    // SAFETY: `elf_mapped_base` points to a fully mapped ELF image, as required
    // from the caller (see UNSAFE_BUFFER_USAGE in header). The PT_NOTE segment
    // described by this program header is mapped at
    // `p_vaddr + relocation_offset` and spans `p_memsz` bytes.
    SpanReader reader(UNSAFE_BUFFERS(span(
        reinterpret_cast<const uint8_t*>(header.p_vaddr + relocation_offset),
        static_cast<size_t>(header.p_memsz))));

    // Look for a NT_GNU_BUILD_ID note with name == "GNU".
    while (reader.remaining() > 0) {
      const std::optional<Nhdr> note = reader.ReadNativeEndian<Nhdr>();
      if (!note) {
        return "";
      }

      // The note header is followed by the name and the descriptor, each
      // padded to a multiple of 4 bytes.
      const std::optional<span<const uint8_t>> name =
          reader.Read(bits::AlignUp(note->n_namesz, static_cast<Word>(4)));
      if (!name) {
        return "";
      }

      // `kGnuNoteName` includes the '\0' character.
      if (note->n_type == NT_GNU_BUILD_ID &&
          note->n_namesz == sizeof(kGnuNoteName) &&
          *name == byte_span_with_nul_from_cstring(kGnuNoteName)) {
        // Validate that the build ID is reasonably sized. If it is not, move on
        // to the next PT_NOTE segment.
        if (note->n_descsz > kMaxBuildIdByteLength) {
          break;
        }

        // Return the build ID as a hex string.
        const std::optional<span<const uint8_t>> build_id_bytes =
            reader.Read(note->n_descsz);
        if (!build_id_bytes) {
          return "";
        }
        return uppercase ? HexEncode(*build_id_bytes)
                         : HexEncodeLower(*build_id_bytes);
      }

      if (!reader.Skip(bits::AlignUp(note->n_descsz, static_cast<Word>(4)))) {
        return "";
      }
    }
  }

  return "";
}

std::optional<std::string_view> ReadElfLibraryName(
    const void* elf_mapped_base) {
  // GetElfProgramHeaders() validates the ELF header and returns an empty span
  // if it is invalid.
  // SAFETY: Required from caller, see UNSAFE_BUFFER_USAGE in header.
  const span<const Phdr> program_headers =
      UNSAFE_BUFFERS(GetElfProgramHeaders(elf_mapped_base));
  const size_t relocation_offset =
      ComputeRelocationOffset(elf_mapped_base, program_headers);
  for (const Phdr& header : program_headers) {
    if (header.p_type != PT_DYNAMIC) {
      continue;
    }

    // SAFETY: `elf_mapped_base` points to a fully mapped ELF image, as required
    // from the caller (see UNSAFE_BUFFER_USAGE in header). The PT_DYNAMIC
    // segment described by this program header is mapped at
    // `p_vaddr + relocation_offset` and spans `p_memsz` bytes, i.e.
    // `p_memsz / sizeof(Dyn)` complete entries.
    const span<const Dyn> dynamic_entries = UNSAFE_BUFFERS(
        span(reinterpret_cast<const Dyn*>(header.p_vaddr + relocation_offset),
             static_cast<size_t>(header.p_memsz) / sizeof(Dyn)));

    // Read through the ELF dynamic sections to find the string table and
    // SONAME offsets, which are used to compute the offset of the library
    // name string.
    Xword soname_strtab_offset = 0;
    uintptr_t strtab_addr = 0;
    for (const Dyn& dynamic_entry : dynamic_entries) {
      if (dynamic_entry.d_tag == DT_STRTAB) {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID) || \
    (defined(ARCH_CPU_RISCV_FAMILY) && BUILDFLAG(IS_LINUX))
        // Fuchsia and Android do not relocate the strtab pointer on ELF load.
        // When the dynamic section is readonly, the strtab pointer is also not
        // relocated. This is the default ABI behavior on RISC-V GNU Linux (See
        // sysdeps/riscv/dl-relocate-ld.h).
        strtab_addr = static_cast<uintptr_t>(dynamic_entry.d_un.d_ptr) +
                      relocation_offset;
#else
        strtab_addr = static_cast<uintptr_t>(dynamic_entry.d_un.d_ptr);
#endif
      } else if (dynamic_entry.d_tag == DT_SONAME) {
        // The Android NDK wrongly defines `d_val` as an Elf32_Sword for 32 bits
        // and thus needs this cast.
        soname_strtab_offset = static_cast<Xword>(dynamic_entry.d_un.d_val);
      }
    }
    if (soname_strtab_offset && strtab_addr) {
      // SAFETY: `elf_mapped_base` points to a fully mapped ELF image, as
      // required from the caller (see UNSAFE_BUFFER_USAGE in header). DT_STRTAB
      // is the mapped address of the dynamic string table, and DT_SONAME is the
      // offset within it of the NUL-terminated library name.
      return std::string_view(UNSAFE_BUFFERS(
          reinterpret_cast<const char*>(strtab_addr) + soname_strtab_offset));
    }
  }

  return std::nullopt;
}

span<const Phdr> GetElfProgramHeaders(const void* elf_mapped_base) {
  const Ehdr* elf_header = GetElfHeader(elf_mapped_base);
  if (!elf_header) {
    return {};
  }

  // SAFETY: `elf_mapped_base` points to a fully mapped ELF image, as required
  // from the caller (see UNSAFE_BUFFER_USAGE in header), and its header was
  // validated above. Per the ELF specification, the program header table
  // starts `e_phoff` bytes from the beginning of the image and holds `e_phnum`
  // entries.
  return UNSAFE_BUFFERS(span(
      reinterpret_cast<const Phdr*>(
          reinterpret_cast<const uint8_t*>(elf_header) + elf_header->e_phoff),
      static_cast<size_t>(elf_header->e_phnum)));
}

size_t GetRelocationOffset(const void* elf_mapped_base) {
  return ComputeRelocationOffset(
      elf_mapped_base,
      // SAFETY: Required from caller, see UNSAFE_BUFFER_USAGE in header.
      UNSAFE_BUFFERS(GetElfProgramHeaders(elf_mapped_base)));
}

}  // namespace base::debug

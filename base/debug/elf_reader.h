// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_ELF_READER_H_
#define BASE_DEBUG_ELF_READER_H_

#include <elf.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"

// Functions for querying metadata from ELF binaries.
//
// All functions take a pointer to the start of an ELF image and read from it at
// offsets taken from the image's own headers. Since they have no way to check
// that this memory is actually mapped (only the ELF header's magic number is
// validated), they are marked `UNSAFE_BUFFER_USAGE`, and callers must justify
// the pointer they pass in.

#if __SIZEOF_POINTER__ == 4
using Phdr = Elf32_Phdr;
#else
using Phdr = Elf64_Phdr;
#endif

namespace base {
namespace debug {

// Hex-encodes the build ID from the ELF binary located at |elf_mapped_base|.
// Returns the build ID, or an empty string if the build ID couldn't be read.
// When |uppercase| is |true|, the output string is written using uppercase hex
// characters. Otherwise, the output is lowercased.
// PRECONDITIONS: `elf_mapped_base` must point to the start of an ELF image that
// is fully mapped into memory, such as a module loaded by the dynamic loader.
UNSAFE_BUFFER_USAGE BASE_EXPORT std::string ReadElfBuildId(
    const void* elf_mapped_base,
    bool uppercase);

// Returns the library name from the ELF file mapped at |elf_mapped_base|.
// Returns an empty result if the name could not be read.
// PRECONDITIONS: Same as ReadElfBuildId() above.
UNSAFE_BUFFER_USAGE BASE_EXPORT std::optional<std::string_view>
ReadElfLibraryName(const void* elf_mapped_base);

// Returns a span of ELF program headers for the ELF file mapped at
// |elf_mapped_base|, or an empty span if the header couldn't be read.
// PRECONDITIONS: Same as ReadElfBuildId() above.
UNSAFE_BUFFER_USAGE BASE_EXPORT span<const Phdr> GetElfProgramHeaders(
    const void* elf_mapped_base);

// Returns the offset to add to virtual addresses in the image to compute the
// mapped virtual address. This value must be added to the p_vaddr field in the
// Phdrs to obtain the mapped virtual address.
// PRECONDITIONS: Same as ReadElfBuildId() above.
UNSAFE_BUFFER_USAGE BASE_EXPORT size_t
GetRelocationOffset(const void* elf_mapped_base);

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_ELF_READER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MACH_O_H_
#define BASE_MAC_MACH_O_H_

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdint.h>

#include <type_traits>

#include "base/base_export.h"
#include "base/files/file_path.h"

namespace base {

// The return value of MachOArchitectures.
enum class MachOArchitectures : uint32_t {
  // Corresponds to cpu_type_t CPU_TYPE_X86_64.
  kX86_64 = 1 << 0,

  // Corresponds to cpu_type_t CPU_TYPE_ARM64.
  kARM64 = 1 << 1,

  // A Mach-O file with an architecture other than one of those listed above.
  kUnknownArchitecture = 1 << 29,

  // Not a Mach-O file. This bit may only appear alone.
  kInvalidFormat = 1 << 30,

  // Not a file at all. This bit may only appear alone.
  kFileError = 1 << 31,
};

// MachOArchitectures is an enum class for better namespacing, but it’s treated
// as a bitfield, so define a few operators to make it easier to work with.

inline constexpr MachOArchitectures operator&(MachOArchitectures a,
                                              MachOArchitectures b) {
  using UnderlyingType = std::underlying_type<MachOArchitectures>::type;
  return static_cast<MachOArchitectures>(static_cast<UnderlyingType>(a) &
                                         static_cast<UnderlyingType>(b));
}

inline constexpr MachOArchitectures operator|(MachOArchitectures a,
                                              MachOArchitectures b) {
  using UnderlyingType = std::underlying_type<MachOArchitectures>::type;
  return static_cast<MachOArchitectures>(static_cast<UnderlyingType>(a) |
                                         static_cast<UnderlyingType>(b));
}

inline constexpr MachOArchitectures operator|=(MachOArchitectures& a,
                                               MachOArchitectures b) {
  a = a | b;
  return a;
}

// Determines the CPU architecture of a Mach-O file, or the CPU architectures of
// a fat file.
//
// This only considers the mach_header[_64]::cputype field of (thin) Mach-O
// files, and the fat_arch[_64]::cputype fields of fat files. For a fat file,
// more than one bit may be set in the return value.
BASE_EXPORT MachOArchitectures GetMachOArchitectures(const FilePath& path);

}  // namespace base

#endif  // BASE_MAC_MACH_O_H_

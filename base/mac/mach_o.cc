// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mach_o.h"

#include <mach/machine.h>

#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/sys_byteorder.h"

namespace base {

namespace {

// ByteSwap is only implemented for unsigned types, and cpu_type_t is a signed
// type.
cpu_type_t ByteSwapCPUType(const cpu_type_t cputype) {
  return static_cast<cpu_type_t>(
      ByteSwap(static_cast<std::make_unsigned<cpu_type_t>::type>(cputype)));
}

MachOArchitectures CPUTypeToBit(const cpu_type_t cputype) {
  switch (cputype) {
    case CPU_TYPE_X86_64:
      return MachOArchitectures::kX86_64;
    case CPU_TYPE_ARM64:
      return MachOArchitectures::kARM64;
    default:
      return MachOArchitectures::kUnknownArchitecture;
  }
}

template <typename FatArchType>
MachOArchitectures GetFatMachOArchitectures(File& file,
                                            const uint32_t nfat_arch,
                                            const bool swap) {
  MachOArchitectures result = static_cast<MachOArchitectures>(0);

  std::vector<FatArchType> archs(nfat_arch);
  if (!file.ReadAtCurrentPosAndCheck(
          as_writable_bytes(make_span(archs.data(), archs.size())))) {
    return MachOArchitectures::kInvalidFormat;
  }

  for (const FatArchType& arch : archs) {
    result |= CPUTypeToBit(swap ? ByteSwapCPUType(arch.cputype) : arch.cputype);
  }

  DCHECK_NE(result, static_cast<MachOArchitectures>(0));

  return result;
}

}  // namespace

MachOArchitectures GetMachOArchitectures(const FilePath& path) {
  File file(path, File::FLAG_OPEN | File::FLAG_READ);
  if (!file.IsValid()) {
    return MachOArchitectures::kFileError;
  }

  union {
    uint32_t magic;

    // In a 64-bit file, the header will be a mach_header_64 instead of a
    // mach_header, but the only difference between the two is that the latter
    // ends with a 4-byte padding field. This function does not consider
    // anything beyond the header, so mach_header will suffice in place of
    // mach_header_64.
    mach_header mach;

    fat_header fat;
  } header;
  if (!file.ReadAtCurrentPosAndCheck(
          as_writable_bytes(make_span(&header.magic, 1)))) {
    return MachOArchitectures::kInvalidFormat;
  }

  switch (header.magic) {
    case MH_MAGIC:
    case MH_MAGIC_64:
    case MH_CIGAM:
    case MH_CIGAM_64: {
      const bool is_64 =
          header.magic == MH_MAGIC_64 || header.magic == MH_CIGAM_64;
      const bool swap = header.magic == MH_CIGAM || header.magic == MH_CIGAM_64;

      if (!file.ReadAtCurrentPosAndCheck(
              as_writable_bytes(make_span(&header.mach, 1))
                  .subspan(sizeof(header.magic)))) {
        return MachOArchitectures::kInvalidFormat;
      }

      if (swap) {
        header.mach.magic = ByteSwap(header.mach.magic);
        header.mach.cputype = ByteSwapCPUType(header.mach.cputype);
      }

      DCHECK(header.mach.magic == MH_MAGIC || header.mach.magic == MH_MAGIC_64);
      DCHECK_EQ(header.mach.magic == MH_MAGIC_64, is_64);

      return CPUTypeToBit(header.mach.cputype);
    }

    case FAT_MAGIC:
    case FAT_MAGIC_64:
    case FAT_CIGAM:
    case FAT_CIGAM_64: {
      const bool is_64 =
          header.magic == FAT_MAGIC_64 || header.magic == FAT_CIGAM_64;
      const bool swap =
          header.magic == FAT_CIGAM || header.magic == FAT_CIGAM_64;

      if (!file.ReadAtCurrentPosAndCheck(
              as_writable_bytes(make_span(&header.fat, 1))
                  .subspan(sizeof(header.magic)))) {
        return MachOArchitectures::kInvalidFormat;
      }

      if (swap) {
        header.fat.magic = ByteSwap(header.fat.magic);
        header.fat.nfat_arch = ByteSwap(header.fat.nfat_arch);
      }

      DCHECK(header.fat.magic == FAT_MAGIC || header.fat.magic == FAT_MAGIC_64);
      DCHECK_EQ(header.fat.magic == FAT_MAGIC_64, is_64);

      if (header.fat.nfat_arch == 0) {
        return MachOArchitectures::kInvalidFormat;
      }

      return is_64 ? GetFatMachOArchitectures<fat_arch_64>(
                         file, header.fat.nfat_arch, swap)
                   : GetFatMachOArchitectures<fat_arch>(
                         file, header.fat.nfat_arch, swap);
    }

    default: {
      return MachOArchitectures::kInvalidFormat;
    }
  }
}

}  // namespace base

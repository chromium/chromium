// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base {

MemoryMappedFile::MemoryMappedFile() = default;

#if !BUILDFLAG(IS_NACL)
bool MemoryMappedFile::MapFileRegionToMemory(
    const MemoryMappedFile::Region& region,
    Access access) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  off_t map_start = 0;
  size_t map_size = 0u;
  int32_t data_offset = 0;
  size_t byte_size = 0u;

  if (region == MemoryMappedFile::Region::kWholeFile) {
    int64_t file_len = file_.GetLength();
    if (file_len < 0) {
      DPLOG(ERROR) << "fstat " << file_.GetPlatformFile();
      return false;
    }
    if (!IsValueInRangeForNumericType<size_t>(file_len))
      return false;
    map_size = base::checked_cast<size_t>(file_len);
    byte_size = map_size;
  } else {
    // The region can be arbitrarily aligned. mmap, instead, requires both the
    // start and size to be page-aligned. Hence, we map here the page-aligned
    // outer region [|aligned_start|, |aligned_start| + |size|] which contains
    // |region| and then add up the |data_offset| displacement.
    int64_t aligned_start = 0;
    size_t aligned_size = 0u;
    CalculateVMAlignedBoundaries(region.offset,
                                 region.size,
                                 &aligned_start,
                                 &aligned_size,
                                 &data_offset);

    // Ensure that the casts in the mmap call below are sane.
    if (aligned_start < 0 ||
        !IsValueInRangeForNumericType<off_t>(aligned_start)) {
      DLOG(ERROR) << "Region bounds are not valid for mmap";
      return false;
    }

    map_start = base::checked_cast<off_t>(aligned_start);
    map_size = aligned_size;
    byte_size = region.size;
  }

  if (map_size == 0u) {
    // mmap() requires `map_size > 0`, and this ensures an empty span indicates
    // invalid.
    return false;
  }

  int prot = 0;
  int flags = MAP_SHARED;
  switch (access) {
    case READ_ONLY:
      prot |= PROT_READ;
      break;

    case READ_WRITE:
      prot |= PROT_READ | PROT_WRITE;
      break;

    case READ_WRITE_COPY:
      prot |= PROT_READ | PROT_WRITE;
      flags = MAP_PRIVATE;
      break;

    case READ_WRITE_EXTEND:
      prot |= PROT_READ | PROT_WRITE;

      if (!AllocateFileRegion(&file_, region.offset, region.size))
        return false;

      break;
  }

  auto* ptr = static_cast<uint8_t*>(
      mmap(nullptr, map_size, prot, flags, file_.GetPlatformFile(), map_start));
  if (ptr == MAP_FAILED) {
    DPLOG(ERROR) << "mmap " << file_.GetPlatformFile();
    return false;
  }

  // SAFETY: For the span construction to be valid, `ptr` needs to point to at
  // least `map_size` many bytes, which is the guarantee of mmap() when it
  // returns a valid pointer, and that `data_offset + byte_size <= map_size`.
  //
  // If the mapping is of the whole file, `map_size == byte_size`
  // and `data_offset == 0`, so `data_offset + byte_size <= map_size` is
  // trivially satisfied.
  //
  // If the mapping is a sub-range of the file:
  // - `aligned_start` is page aligned and <= `start`.
  // - `map_size` is a multiple of the VM granularity and >=
  //   `byte_size`.
  // - `data_offset` is the displacement of `start` w.r.t `aligned_start`.
  // |..................|xxxxxxxxxxxxxxxxxx|.................|
  // ^ aligned start    ^ start            |                 |
  // ^------------------^ data_offset      |                 |
  //                    ^------------------^ byte_size       |
  // ^-------------------------------------------------------^ map_size
  //
  // The `data_offset` undoes the alignment of start. The `map_size` contains
  // the padding before and after the mapped region to satisfy alignment. So
  // the `data_offset + byte_size <= map_size`.
  bytes_ = UNSAFE_BUFFERS(base::span(ptr + data_offset, byte_size));
  return true;
}
#endif

void MemoryMappedFile::CloseHandles() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (!bytes_.empty()) {
    munmap(bytes_.data(), bytes_.size());
  }
  file_.Close();
  bytes_ = base::span<uint8_t>();
}

}  // namespace base

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <winnt.h>

#include <limits>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/pe_image.h"

namespace base {

MemoryMappedFile::MemoryMappedFile() = default;

bool MemoryMappedFile::MapImageToMemory(Access access) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // The arguments to the calls of ::CreateFile(), ::CreateFileMapping(), and
  // ::MapViewOfFile() need to be self consistent as far as access rights and
  // type of mapping or one or more of them will fail in non-obvious ways.

  if (!file_.IsValid())
    return false;

  file_mapping_.Set(::CreateFileMapping(file_.GetPlatformFile(), nullptr,
                                        PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0,
                                        0, NULL));
  if (!file_mapping_.is_valid())
    return false;

  auto* ptr = static_cast<uint8_t*>(
      ::MapViewOfFile(file_mapping_.get(), FILE_MAP_READ, 0, 0, 0));
  if (!ptr) {
    return false;
  }

  // We need to know how large the mapped file is.
  base::win::PEImage pe_image(ptr);
  size_t len = pe_image.GetNTHeaders()->OptionalHeader.SizeOfImage;
  if (len == 0u) {
    // Consistent cross-platform behaviour, an empty `bytes_` indicates nothing
    // is mapped.
    return false;
  }

  // SAFETY: The `len` is the size of the image at `ptr`.
  bytes_ = UNSAFE_BUFFERS(base::span(ptr, len));
  return true;
}

bool MemoryMappedFile::MapFileRegionToMemory(
    const MemoryMappedFile::Region& region,
    Access access) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  DCHECK(access != READ_CODE_IMAGE || region == Region::kWholeFile);

  if (!file_.IsValid())
    return false;

  DWORD view_access;
  DWORD flags = 0;
  ULARGE_INTEGER size = {};
  switch (access) {
    case READ_ONLY:
      flags |= PAGE_READONLY;
      view_access = FILE_MAP_READ;
      break;
    case READ_WRITE:
      flags |= PAGE_READWRITE;
      view_access = FILE_MAP_WRITE;
      break;
    case READ_WRITE_COPY:
      flags |= PAGE_WRITECOPY;
      view_access = FILE_MAP_COPY;
      break;
    case READ_WRITE_EXTEND:
      flags |= PAGE_READWRITE;
      view_access = FILE_MAP_WRITE;
      size.QuadPart = region.size;
      break;
    case READ_CODE_IMAGE:
      return MapImageToMemory(access);
  }

  file_mapping_.Set(::CreateFileMapping(file_.GetPlatformFile(), NULL, flags,
                                        size.HighPart, size.LowPart, NULL));
  if (!file_mapping_.is_valid())
    return false;

  ULARGE_INTEGER map_start = {};
  SIZE_T map_size = 0u;
  int32_t data_offset = 0;
  size_t byte_size = 0u;

  if (region == MemoryMappedFile::Region::kWholeFile) {
    DCHECK_NE(READ_WRITE_EXTEND, access);
    int64_t file_len = file_.GetLength();
    if (file_len <= 0 || !IsValueInRangeForNumericType<size_t>(file_len)) {
      return false;
    }
    byte_size = base::checked_cast<size_t>(file_len);
  } else {
    // The region can be arbitrarily aligned. MapViewOfFile, instead, requires
    // that the start address is aligned to the VM granularity (which is
    // typically larger than a page size, for instance 32k).
    // Also, conversely to POSIX's mmap, the |map_size| doesn't have to be
    // aligned and must be less than or equal the mapped file size.
    // We map here the outer region [|aligned_start|, |aligned_start+size|]
    // which contains |region| and then add up the |data_offset| displacement.
    int64_t aligned_start = 0;
    size_t ignored = 0u;
    CalculateVMAlignedBoundaries(region.offset, region.size, &aligned_start,
                                 &ignored, &data_offset);
    base::CheckedNumeric<SIZE_T> full_map_size = region.size;
    full_map_size += data_offset;

    // Ensure that the casts below in the MapViewOfFile call are sane.
    if (aligned_start < 0 || !full_map_size.IsValid()) {
      DLOG(ERROR) << "Region bounds are not valid for MapViewOfFile";
      return false;
    }
    map_start.QuadPart = static_cast<uint64_t>(aligned_start);
    map_size = full_map_size.ValueOrDie();
    byte_size = region.size;

    if (map_size == 0u) {
      // Consistent cross-platform behaviour, an empty `bytes_` indicates
      // nothing is mapped.
      return false;
    }
  }

  auto* ptr = static_cast<uint8_t*>(
      ::MapViewOfFile(file_mapping_.get(), view_access, map_start.HighPart,
                      map_start.LowPart, map_size));
  if (ptr == nullptr) {
    return false;
  }

  // SAFETY: For the span construction to be valid, `ptr` needs to point to at
  // least `data_size + byte_size` many bytes. The MapViewOfFile() will return a
  // pointer of `map_size` bytes, unless it's 0 in which case it returns a
  // pointer to all bytes in the file after the offset.
  //
  // If the mapping is of the whole file, `map_size == 0`, so `file_len` bytes
  // are mapped. `byte_size == file_len` and `data_offset == 0`, so
  // `data_offset + byte_size <= file_len` is trivially satisfied.
  //
  // If the mapping is a sub-range of the file, `map_size > 0` and `map_size`
  // many bytes are mapped:
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

void MemoryMappedFile::CloseHandles() {
  if (!bytes_.empty()) {
    ::UnmapViewOfFile(bytes_.data());
  }
  if (file_mapping_.is_valid())
    file_mapping_.Close();
  if (file_.IsValid())
    file_.Close();

  bytes_ = base::span<uint8_t>();
}

}  // namespace base

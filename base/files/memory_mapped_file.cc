// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

namespace base {

const MemoryMappedFile::Region MemoryMappedFile::Region::kWholeFile = {0, 0};

MemoryMappedFile::~MemoryMappedFile() {
  CloseHandles();
}

#if !BUILDFLAG(IS_NACL)
bool MemoryMappedFile::Initialize(const FilePath& file_name, Access access) {
  if (IsValid())
    return false;

  uint32_t flags = 0;
  switch (access) {
    case READ_ONLY:
      flags = File::FLAG_OPEN | File::FLAG_READ;
      break;
    case READ_WRITE_COPY:
      flags = File::FLAG_OPEN | File::FLAG_READ;
#if BUILDFLAG(IS_FUCHSIA)
      // Fuchsia's mmap() implementation does not allow us to create a
      // copy-on-write mapping of a file opened as read-only.
      flags |= File::FLAG_WRITE;
#endif
      break;
    case READ_WRITE:
      flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
      break;
    case READ_WRITE_EXTEND:
      // Can't open with "extend" because no maximum size is known.
      NOTREACHED();
#if BUILDFLAG(IS_WIN)
    case READ_CODE_IMAGE:
      flags |= File::FLAG_OPEN | File::FLAG_READ |
               File::FLAG_WIN_EXCLUSIVE_WRITE | File::FLAG_WIN_EXECUTE;
      break;
#endif
  }
  file_.Initialize(file_name, flags);

  if (!file_.IsValid()) {
    DLOG(ERROR) << "Couldn't open " << file_name.AsUTF8Unsafe();
    return false;
  }

  if (!MapFileRegionToMemory(Region::kWholeFile, access)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::Initialize(File file, Access access) {
  DCHECK_NE(READ_WRITE_EXTEND, access);
  return Initialize(std::move(file), Region::kWholeFile, access);
}

bool MemoryMappedFile::Initialize(File file,
                                  const Region& region,
                                  Access access) {
  switch (access) {
    case READ_WRITE_EXTEND:
      DCHECK(Region::kWholeFile != region);
      {
        CheckedNumeric<int64_t> region_end(region.offset);
        region_end += region.size;
        if (!region_end.IsValid()) {
          DLOG(ERROR) << "Region bounds exceed maximum for base::File.";
          return false;
        }
      }
      [[fallthrough]];
    case READ_ONLY:
    case READ_WRITE:
    case READ_WRITE_COPY:
      // Ensure that the region values are valid.
      if (region.offset < 0) {
        DLOG(ERROR) << "Region bounds are not valid.";
        return false;
      }
      break;
#if BUILDFLAG(IS_WIN)
    case READ_CODE_IMAGE:
      DCHECK(Region::kWholeFile == region);
      break;
#endif
  }

  if (IsValid())
    return false;

  if (region != Region::kWholeFile)
    DCHECK_GE(region.offset, 0);

  file_ = std::move(file);

  if (!MapFileRegionToMemory(region, access)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::IsValid() const {
  return !bytes_.empty();
}

// static
void MemoryMappedFile::CalculateVMAlignedBoundaries(int64_t start,
                                                    size_t size,
                                                    int64_t* aligned_start,
                                                    size_t* aligned_size,
                                                    int32_t* offset) {
  // Sadly, on Windows, the mmap alignment is not just equal to the page size.
  uint64_t mask = SysInfo::VMAllocationGranularity() - 1;
  CHECK(IsValueInRangeForNumericType<int32_t>(mask));
  *offset = static_cast<int32_t>(static_cast<uint64_t>(start) & mask);
  *aligned_start = static_cast<int64_t>(static_cast<uint64_t>(start) & ~mask);
  // The CHECK above means bit 31 is not set in `mask`, which in turn means
  // *offset is positive.  Therefore casting it to a size_t is safe.
  *aligned_size =
      (size + static_cast<size_t>(*offset) + static_cast<size_t>(mask)) & ~mask;
}
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace base

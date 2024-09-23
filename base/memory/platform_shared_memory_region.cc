// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/bits.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"

namespace base {
namespace subtle {

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::CreateWritable(
    size_t size) {
  return Create(Mode::kWritable, size);
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::CreateUnsafe(
    size_t size) {
  return Create(Mode::kUnsafe, size);
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion() = default;
PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    PlatformSharedMemoryRegion&& other) = default;
PlatformSharedMemoryRegion& PlatformSharedMemoryRegion::operator=(
    PlatformSharedMemoryRegion&& other) = default;
PlatformSharedMemoryRegion::~PlatformSharedMemoryRegion() = default;

ScopedPlatformSharedMemoryHandle
PlatformSharedMemoryRegion::PassPlatformHandle() {
  return std::move(handle_);
}

std::optional<span<uint8_t>> PlatformSharedMemoryRegion::MapAt(
    uint64_t offset,
    size_t size,
    SharedMemoryMapper* mapper) const {
  if (!IsValid())
    return std::nullopt;

  if (size == 0)
    return std::nullopt;

  size_t end_byte;
  if (!CheckAdd(offset, size).AssignIfValid(&end_byte) || end_byte > size_) {
    return std::nullopt;
  }

  // TODO(dcheng): Presumably the actual size of the mapping is rounded to
  // `SysInfo::VMAllocationGranularity()`. Should this accounting be done with
  // that in mind?
  if (!SharedMemorySecurityPolicy::AcquireReservationForMapping(size)) {
    return std::nullopt;
  }

  if (!mapper)
    mapper = SharedMemoryMapper::GetDefaultInstance();

  // The backing mapper expects offset to be aligned to
  // `SysInfo::VMAllocationGranularity()`.
  uint64_t aligned_offset =
      bits::AlignDown(offset, uint64_t{SysInfo::VMAllocationGranularity()});
  size_t adjustment_for_alignment =
      static_cast<size_t>(offset - aligned_offset);

  bool write_allowed = mode_ != Mode::kReadOnly;
  auto result = mapper->Map(GetPlatformHandle(), write_allowed, aligned_offset,
                            size + adjustment_for_alignment);

  if (result.has_value()) {
    DCHECK(IsAligned(result.value().data(), kMapMinimumAlignment));
    if (offset != 0) {
      // Undo the previous adjustment so the returned mapping respects the exact
      // requested `offset` and `size`.
      result = result->subspan(adjustment_for_alignment);
    }
  } else {
    SharedMemorySecurityPolicy::ReleaseReservationForMapping(size);
  }

  return result;
}

void PlatformSharedMemoryRegion::Unmap(span<uint8_t> mapping,
                                       SharedMemoryMapper* mapper) {
  if (!mapper) {
    mapper = SharedMemoryMapper::GetDefaultInstance();
  }

  mapper->Unmap(mapping);

  SharedMemorySecurityPolicy::ReleaseReservationForMapping(mapping.size());
}

}  // namespace subtle
}  // namespace base

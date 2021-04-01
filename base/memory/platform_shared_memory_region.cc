// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/memory/aligned_memory.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"

namespace base {
namespace subtle {

namespace {

void RecordMappingWasBlockedHistogram(bool blocked) {
  UmaHistogramBoolean("SharedMemory.MapBlockedForSecurity", blocked);
}

}  // namespace

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

PlatformSharedMemoryRegion::ScopedPlatformHandle
PlatformSharedMemoryRegion::PassPlatformHandle() {
  return std::move(handle_);
}

bool PlatformSharedMemoryRegion::MapAt(off_t offset,
                                       size_t size,
                                       void** memory,
                                       size_t* mapped_size) const {
  if (!IsValid())
    return false;

  if (size == 0)
    return false;

  size_t end_byte;
  if (!CheckAdd(offset, size).AssignIfValid(&end_byte) || end_byte > size_) {
    return false;
  }

  if (!SharedMemorySecurityPolicy::AcquireReservationForMapping(size)) {
    RecordMappingWasBlockedHistogram(/*blocked=*/true);
    return false;
  }

  RecordMappingWasBlockedHistogram(/*blocked=*/false);

  bool success = MapAtInternal(offset, size, memory, mapped_size);
  if (success) {
    DCHECK(IsAligned(*memory, kMapMinimumAlignment));
  } else {
    SharedMemorySecurityPolicy::ReleaseReservationForMapping(size);
  }

  return success;
}

}  // namespace subtle
}  // namespace base

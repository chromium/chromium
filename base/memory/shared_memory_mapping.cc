// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/shared_memory_mapping.h"

#include <cstdint>
#include <utility>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/system/sys_info.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

namespace base {

SharedMemoryMapping::SharedMemoryMapping() = default;

SharedMemoryMapping::SharedMemoryMapping(SharedMemoryMapping&& mapping) noexcept
    : mapped_span_(std::exchange(mapping.mapped_span_, span<uint8_t>())),
      size_(mapping.size_),
      guid_(mapping.guid_),
      mapper_(mapping.mapper_) {}

SharedMemoryMapping& SharedMemoryMapping::operator=(
    SharedMemoryMapping&& mapping) noexcept {
  Unmap();
  mapped_span_ = std::exchange(mapping.mapped_span_, span<uint8_t>());
  size_ = mapping.size_;
  guid_ = mapping.guid_;
  mapper_ = mapping.mapper_;
  return *this;
}

SharedMemoryMapping::~SharedMemoryMapping() {
  Unmap();
}

SharedMemoryMapping::SharedMemoryMapping(span<uint8_t> mapped_span,
                                         size_t size,
                                         const UnguessableToken& guid,
                                         SharedMemoryMapper* mapper)
    : mapped_span_(mapped_span), size_(size), guid_(guid), mapper_(mapper) {
  CHECK_LE(size_, mapped_span_.size());
  // Note: except on Windows, `mapped_span_.size() == size_`.
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
}

void SharedMemoryMapping::Unmap() {
  if (!IsValid())
    return;

  SharedMemorySecurityPolicy::ReleaseReservationForMapping(size_);
  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);

  SharedMemoryMapper* mapper = mapper_;
  if (!mapper)
    mapper = SharedMemoryMapper::GetDefaultInstance();

  // The backing mapper expects offset to be aligned to
  // `SysInfo::VMAllocationGranularity()`, so replicate the alignment that was
  // done when originally mapping in the region.
  uint8_t* aligned_data =
      bits::AlignDown(mapped_span_.data(), SysInfo::VMAllocationGranularity());
  size_t adjusted_size =
      mapped_span_.size() +
      static_cast<size_t>(mapped_span_.data() - aligned_data);
  span<uint8_t> span_to_unmap = make_span(aligned_data, adjusted_size);
  mapper->Unmap(span_to_unmap);
}

ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping() = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    ReadOnlySharedMemoryMapping&&) noexcept = default;
ReadOnlySharedMemoryMapping& ReadOnlySharedMemoryMapping::operator=(
    ReadOnlySharedMemoryMapping&&) noexcept = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    span<uint8_t> mapped_span,
    size_t size,
    const UnguessableToken& guid,
    SharedMemoryMapper* mapper)
    : SharedMemoryMapping(mapped_span, size, guid, mapper) {}

WritableSharedMemoryMapping::WritableSharedMemoryMapping() = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    WritableSharedMemoryMapping&&) noexcept = default;
WritableSharedMemoryMapping& WritableSharedMemoryMapping::operator=(
    WritableSharedMemoryMapping&&) noexcept = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    span<uint8_t> mapped_span,
    size_t size,
    const UnguessableToken& guid,
    SharedMemoryMapper* mapper)
    : SharedMemoryMapping(mapped_span, size, guid, mapper) {}

}  // namespace base

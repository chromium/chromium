// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_mapping.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/memory/shared_memory_tracker.h"
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
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
}

void SharedMemoryMapping::Unmap() {
  if (!IsValid())
    return;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);

  SharedMemoryMapper* mapper = mapper_;
  if (!mapper)
    mapper = SharedMemoryMapper::GetDefaultInstance();
  mapper->Unmap(mapped_span_);
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

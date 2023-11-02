// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/unsafe_shared_memory_region.h"

#include <utility>

#include "base/check_op.h"

namespace base {

UnsafeSharedMemoryRegion::CreateFunction*
    UnsafeSharedMemoryRegion::create_hook_ = nullptr;

// static
UnsafeSharedMemoryRegion UnsafeSharedMemoryRegion::Create(size_t size) {
  if (create_hook_)
    return create_hook_(size);

  subtle::PlatformSharedMemoryRegion handle =
      subtle::PlatformSharedMemoryRegion::CreateUnsafe(size);

  return UnsafeSharedMemoryRegion(std::move(handle));
}

// static
UnsafeSharedMemoryRegion UnsafeSharedMemoryRegion::Deserialize(
    subtle::PlatformSharedMemoryRegion handle) {
  return UnsafeSharedMemoryRegion(std::move(handle));
}

// static
subtle::PlatformSharedMemoryRegion
UnsafeSharedMemoryRegion::TakeHandleForSerialization(
    UnsafeSharedMemoryRegion region) {
  return std::move(region.handle_);
}

UnsafeSharedMemoryRegion::UnsafeSharedMemoryRegion() = default;
UnsafeSharedMemoryRegion::UnsafeSharedMemoryRegion(
    UnsafeSharedMemoryRegion&& region) = default;
UnsafeSharedMemoryRegion& UnsafeSharedMemoryRegion::operator=(
    UnsafeSharedMemoryRegion&& region) = default;
UnsafeSharedMemoryRegion::~UnsafeSharedMemoryRegion() = default;

UnsafeSharedMemoryRegion UnsafeSharedMemoryRegion::Duplicate() const {
  return UnsafeSharedMemoryRegion(handle_.Duplicate());
}

WritableSharedMemoryMapping UnsafeSharedMemoryRegion::Map(
    SharedMemoryMapper* mapper) const {
  return MapAt(0, handle_.GetSize(), mapper);
}

WritableSharedMemoryMapping UnsafeSharedMemoryRegion::MapAt(
    uint64_t offset,
    size_t size,
    SharedMemoryMapper* mapper) const {
  if (!IsValid())
    return {};

  auto result = handle_.MapAt(offset, size, mapper);
  if (!result.has_value())
    return {};

  return WritableSharedMemoryMapping(result.value(), size, handle_.GetGUID(),
                                     mapper);
}

bool UnsafeSharedMemoryRegion::IsValid() const {
  return handle_.IsValid();
}

UnsafeSharedMemoryRegion::UnsafeSharedMemoryRegion(
    subtle::PlatformSharedMemoryRegion handle)
    : handle_(std::move(handle)) {
  if (handle_.IsValid()) {
    CHECK_EQ(handle_.GetMode(),
             subtle::PlatformSharedMemoryRegion::Mode::kUnsafe);
  }
}

}  // namespace base

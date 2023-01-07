// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/writable_shared_memory_region.h"

#include <utility>

#include "build/build_config.h"

namespace base {

WritableSharedMemoryRegion::CreateFunction*
    WritableSharedMemoryRegion::create_hook_ = nullptr;

// static
WritableSharedMemoryRegion WritableSharedMemoryRegion::Create(size_t size) {
  if (create_hook_)
    return create_hook_(size);

  subtle::PlatformSharedMemoryRegion handle =
      subtle::PlatformSharedMemoryRegion::CreateWritable(size);

  return WritableSharedMemoryRegion(std::move(handle));
}

// static
WritableSharedMemoryRegion WritableSharedMemoryRegion::Deserialize(
    subtle::PlatformSharedMemoryRegion handle) {
  return WritableSharedMemoryRegion(std::move(handle));
}

// static
subtle::PlatformSharedMemoryRegion
WritableSharedMemoryRegion::TakeHandleForSerialization(
    WritableSharedMemoryRegion region) {
  return std::move(region.handle_);
}

// static
ReadOnlySharedMemoryRegion WritableSharedMemoryRegion::ConvertToReadOnly(
    WritableSharedMemoryRegion region) {
  subtle::PlatformSharedMemoryRegion handle = std::move(region.handle_);
  if (!handle.ConvertToReadOnly())
    return {};

  return ReadOnlySharedMemoryRegion::Deserialize(std::move(handle));
}

UnsafeSharedMemoryRegion WritableSharedMemoryRegion::ConvertToUnsafe(
    WritableSharedMemoryRegion region) {
  subtle::PlatformSharedMemoryRegion handle = std::move(region.handle_);
  if (!handle.ConvertToUnsafe())
    return {};

  return UnsafeSharedMemoryRegion::Deserialize(std::move(handle));
}

WritableSharedMemoryRegion::WritableSharedMemoryRegion() = default;
WritableSharedMemoryRegion::WritableSharedMemoryRegion(
    WritableSharedMemoryRegion&& region) = default;
WritableSharedMemoryRegion& WritableSharedMemoryRegion::operator=(
    WritableSharedMemoryRegion&& region) = default;
WritableSharedMemoryRegion::~WritableSharedMemoryRegion() = default;

WritableSharedMemoryMapping WritableSharedMemoryRegion::Map(
    SharedMemoryMapper* mapper) const {
  return MapAt(0, handle_.GetSize(), mapper);
}

WritableSharedMemoryMapping WritableSharedMemoryRegion::MapAt(
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

bool WritableSharedMemoryRegion::IsValid() const {
  return handle_.IsValid();
}

WritableSharedMemoryRegion::WritableSharedMemoryRegion(
    subtle::PlatformSharedMemoryRegion handle)
    : handle_(std::move(handle)) {
  if (handle_.IsValid()) {
    CHECK_EQ(handle_.GetMode(),
             subtle::PlatformSharedMemoryRegion::Mode::kWritable);
  }
}

}  // namespace base

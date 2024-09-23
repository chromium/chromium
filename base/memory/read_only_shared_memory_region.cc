// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/read_only_shared_memory_region.h"

#include <utility>

#include "build/build_config.h"

namespace base {

ReadOnlySharedMemoryRegion::CreateFunction*
    ReadOnlySharedMemoryRegion::create_hook_ = nullptr;

// static
MappedReadOnlyRegion ReadOnlySharedMemoryRegion::Create(
    size_t size,
    SharedMemoryMapper* mapper) {
  if (create_hook_)
    return create_hook_(size, mapper);

  subtle::PlatformSharedMemoryRegion handle =
      subtle::PlatformSharedMemoryRegion::CreateWritable(size);
  if (!handle.IsValid())
    return {};

  auto result = handle.MapAt(0, handle.GetSize(), mapper);
  if (!result.has_value())
    return {};

  WritableSharedMemoryMapping mapping(result.value(), size, handle.GetGUID(),
                                      mapper);
#if BUILDFLAG(IS_MAC)
  handle.ConvertToReadOnly(mapping.data());
#else
  handle.ConvertToReadOnly();
#endif  // BUILDFLAG(IS_MAC)
  ReadOnlySharedMemoryRegion region(std::move(handle));

  if (!region.IsValid() || !mapping.IsValid())
    return {};

  return {std::move(region), std::move(mapping)};
}

// static
ReadOnlySharedMemoryRegion ReadOnlySharedMemoryRegion::Deserialize(
    subtle::PlatformSharedMemoryRegion handle) {
  return ReadOnlySharedMemoryRegion(std::move(handle));
}

// static
subtle::PlatformSharedMemoryRegion
ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
    ReadOnlySharedMemoryRegion region) {
  return std::move(region.handle_);
}

ReadOnlySharedMemoryRegion::ReadOnlySharedMemoryRegion() = default;
ReadOnlySharedMemoryRegion::ReadOnlySharedMemoryRegion(
    ReadOnlySharedMemoryRegion&& region) = default;
ReadOnlySharedMemoryRegion& ReadOnlySharedMemoryRegion::operator=(
    ReadOnlySharedMemoryRegion&& region) = default;
ReadOnlySharedMemoryRegion::~ReadOnlySharedMemoryRegion() = default;

ReadOnlySharedMemoryRegion ReadOnlySharedMemoryRegion::Duplicate() const {
  return ReadOnlySharedMemoryRegion(handle_.Duplicate());
}

ReadOnlySharedMemoryMapping ReadOnlySharedMemoryRegion::Map(
    SharedMemoryMapper* mapper) const {
  return MapAt(0, handle_.GetSize(), mapper);
}

ReadOnlySharedMemoryMapping ReadOnlySharedMemoryRegion::MapAt(
    uint64_t offset,
    size_t size,
    SharedMemoryMapper* mapper) const {
  if (!IsValid())
    return {};

  auto result = handle_.MapAt(offset, size, mapper);
  if (!result.has_value())
    return {};

  return ReadOnlySharedMemoryMapping(result.value(), size, handle_.GetGUID(),
                                     mapper);
}

bool ReadOnlySharedMemoryRegion::IsValid() const {
  return handle_.IsValid();
}

ReadOnlySharedMemoryRegion::ReadOnlySharedMemoryRegion(
    subtle::PlatformSharedMemoryRegion handle)
    : handle_(std::move(handle)) {
  if (handle_.IsValid()) {
    CHECK_EQ(handle_.GetMode(),
             subtle::PlatformSharedMemoryRegion::Mode::kReadOnly);
  }
}

}  // namespace base

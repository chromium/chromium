// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_shared_memory.h"

#include "base/memory/shared_memory_mapping.h"

namespace base {

HistogramSharedMemory::HistogramSharedMemory() = default;
HistogramSharedMemory::~HistogramSharedMemory() = default;
HistogramSharedMemory::HistogramSharedMemory(HistogramSharedMemory&& other) =
    default;
HistogramSharedMemory& HistogramSharedMemory::operator=(
    HistogramSharedMemory&& other) = default;

// static
absl::optional<HistogramSharedMemory> HistogramSharedMemory::Create(
    int unique_process_id,
    const HistogramSharedMemoryConfig& config) {
  auto shared_memory_region =
      base::WritableSharedMemoryRegion::Create(config.memory_size_bytes);
  if (!shared_memory_region.IsValid()) {
    return absl::nullopt;
  }

  auto shared_memory_mapping = shared_memory_region.Map();
  if (!shared_memory_mapping.IsValid()) {
    return absl::nullopt;
  }

  auto metrics_allocator =
      std::make_unique<base::WritableSharedPersistentMemoryAllocator>(
          std::move(shared_memory_mapping),
          static_cast<uint64_t>(unique_process_id), config.allocator_name);

  return HistogramSharedMemory{std::move(shared_memory_region),
                               std::move(metrics_allocator)};
}

bool HistogramSharedMemory::IsValid() const {
  return region_.IsValid() && allocator_ != nullptr;
}

base::WritableSharedMemoryRegion HistogramSharedMemory::TakeRegion() {
  return std::move(region_);
}

std::unique_ptr<base::WritableSharedPersistentMemoryAllocator>
HistogramSharedMemory::TakeAllocator() {
  return std::move(allocator_);
}

HistogramSharedMemory::HistogramSharedMemory(
    base::WritableSharedMemoryRegion region,
    std::unique_ptr<base::WritableSharedPersistentMemoryAllocator> allocator)
    : region_(std::move(region)), allocator_(std::move(allocator)) {
  CHECK(IsValid());
}

}  // namespace base

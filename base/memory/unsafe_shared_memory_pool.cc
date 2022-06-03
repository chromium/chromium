// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/unsafe_shared_memory_pool.h"

#include "base/logging.h"

namespace {
constexpr size_t kMaxStoredBuffers = 32;
}  // namespace

namespace base {

UnsafeSharedMemoryPool::UnsafeSharedMemoryPool() = default;

UnsafeSharedMemoryPool::~UnsafeSharedMemoryPool() = default;

UnsafeSharedMemoryPool::Handle::Handle(
    PassKey<UnsafeSharedMemoryPool>,
    UnsafeSharedMemoryRegion region,
    WritableSharedMemoryMapping mapping,
    scoped_refptr<UnsafeSharedMemoryPool> pool)
    : region_(std::move(region)),
      mapping_(std::move(mapping)),
      pool_(std::move(pool)) {
  CHECK(pool_);
  DCHECK(region_.IsValid());
  DCHECK(mapping_.IsValid());
}

UnsafeSharedMemoryPool::Handle::~Handle() {
  pool_->ReleaseBuffer(std::move(region_), std::move(mapping_));
}

const UnsafeSharedMemoryRegion& UnsafeSharedMemoryPool::Handle::GetRegion()
    const {
  return region_;
}

const WritableSharedMemoryMapping& UnsafeSharedMemoryPool::Handle::GetMapping()
    const {
  return mapping_;
}

std::unique_ptr<UnsafeSharedMemoryPool::Handle>
UnsafeSharedMemoryPool::MaybeAllocateBuffer(size_t region_size) {
  AutoLock lock(lock_);

  DCHECK_GE(region_size, 0u);
  if (is_shutdown_)
    return nullptr;

  // Only change the configured size if bigger region is requested to avoid
  // unncecessary reallocations.
  if (region_size > region_size_) {
    regions_.clear();
    region_size_ = region_size;
  }
  if (!regions_.empty()) {
    auto region = std::move(regions_.back());
    regions_.pop_back();
    DCHECK_GE(region.first.GetSize(), region_size_);
    auto handle = std::make_unique<Handle>(PassKey<UnsafeSharedMemoryPool>(),
                                           std::move(region.first),
                                           std::move(region.second), this);
    return handle;
  }

  auto region = UnsafeSharedMemoryRegion::Create(region_size_);
  if (!region.IsValid())
    return nullptr;

  WritableSharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return nullptr;

  return std::make_unique<Handle>(PassKey<UnsafeSharedMemoryPool>(),
                                  std::move(region), std::move(mapping), this);
}

void UnsafeSharedMemoryPool::Shutdown() {
  AutoLock lock(lock_);
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;
  regions_.clear();
}

void UnsafeSharedMemoryPool::ReleaseBuffer(
    UnsafeSharedMemoryRegion region,
    WritableSharedMemoryMapping mapping) {
  AutoLock lock(lock_);
  // Only return regions which are at least as big as the current configuration.
  if (is_shutdown_ || regions_.size() >= kMaxStoredBuffers ||
      !region.IsValid() || region.GetSize() < region_size_) {
    DLOG(WARNING) << "Not returning SharedMemoryRegion to the pool:"
                  << " is_shutdown: " << (is_shutdown_ ? "true" : "false")
                  << " stored regions: " << regions_.size()
                  << " configured size: " << region_size_
                  << " this region size: " << region.GetSize()
                  << " valid: " << (region.IsValid() ? "true" : "false");
    return;
  }
  regions_.emplace_back(std::move(region), std::move(mapping));
}

}  // namespace base

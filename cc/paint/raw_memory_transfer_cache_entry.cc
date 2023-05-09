// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/raw_memory_transfer_cache_entry.h"

#include <string.h>
#include <utility>

#include "base/check_op.h"

namespace cc {

ClientRawMemoryTransferCacheEntry::ClientRawMemoryTransferCacheEntry(
    std::vector<uint8_t> data)
    : id_(s_next_id_.GetNext()), data_(std::move(data)) {
  DCHECK_LE(data_.size(), UINT32_MAX);
}

ClientRawMemoryTransferCacheEntry::~ClientRawMemoryTransferCacheEntry() =
    default;

// static
base::AtomicSequenceNumber ClientRawMemoryTransferCacheEntry::s_next_id_;

uint32_t ClientRawMemoryTransferCacheEntry::SerializedSize() const {
  return static_cast<uint32_t>(data_.size());
}

uint32_t ClientRawMemoryTransferCacheEntry::Id() const {
  return id_;
}

bool ClientRawMemoryTransferCacheEntry::Serialize(
    base::span<uint8_t> data) const {
  if (data.size() < data_.size())
    return false;

  memcpy(data.data(), data_.data(), data_.size());
  return true;
}

ServiceRawMemoryTransferCacheEntry::ServiceRawMemoryTransferCacheEntry() =
    default;
ServiceRawMemoryTransferCacheEntry::~ServiceRawMemoryTransferCacheEntry() =
    default;

size_t ServiceRawMemoryTransferCacheEntry::CachedSize() const {
  return data_.size();
}

bool ServiceRawMemoryTransferCacheEntry::Deserialize(
    GrDirectContext* context,
    skgpu::graphite::Recorder* graphite_recorder,
    base::span<const uint8_t> data) {
  data_ = std::vector<uint8_t>(data.begin(), data.end());
  return true;
}

}  // namespace cc

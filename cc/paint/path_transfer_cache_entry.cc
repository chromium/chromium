// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/path_transfer_cache_entry.h"

namespace cc {

ClientPathTransferCacheEntry::ClientPathTransferCacheEntry(const SkPath& path)
    : path_(path) {
  size_ = path_.writeToMemory(nullptr);
}

ClientPathTransferCacheEntry::~ClientPathTransferCacheEntry() = default;

uint32_t ClientPathTransferCacheEntry::Id() const {
  return path_.getGenerationID();
}

size_t ClientPathTransferCacheEntry::SerializedSize() const {
  return size_;
}

bool ClientPathTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), size_);

  size_t bytes_written = path_.writeToMemory(data.data());
  DCHECK_LE(bytes_written, size_);
  return true;
}

ServicePathTransferCacheEntry::ServicePathTransferCacheEntry() = default;

ServicePathTransferCacheEntry::~ServicePathTransferCacheEntry() = default;

size_t ServicePathTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServicePathTransferCacheEntry::Deserialize(
    GrContext* context,
    base::span<const uint8_t> data) {
  size_t read_bytes = path_.readFromMemory(data.data(), data.size());
  // Invalid path.
  if (read_bytes == 0)
    return false;
  if (read_bytes > data.size())
    return false;
  size_ = read_bytes;
  return true;
}

}  // namespace cc

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/textblob_transfer_cache_entry.h"

namespace cc {

ServiceTextBlobTransferCacheEntry::ServiceTextBlobTransferCacheEntry(
    sk_sp<SkTextBlob> blob,
    size_t size)
    : blob_(std::move(blob)), size_(size) {}

ServiceTextBlobTransferCacheEntry::~ServiceTextBlobTransferCacheEntry() =
    default;

size_t ServiceTextBlobTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceTextBlobTransferCacheEntry::Deserialize(
    GrContext* context,
    base::span<const uint8_t> data) {
  NOTREACHED();
  return false;
}

}  // namespace cc

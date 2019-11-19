// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/shader_transfer_cache_entry.h"

namespace cc {

ServiceShaderTransferCacheEntry::ServiceShaderTransferCacheEntry(
    sk_sp<PaintShader> shader,
    size_t size)
    : shader_(std::move(shader)),
      size_(size) {}

ServiceShaderTransferCacheEntry::~ServiceShaderTransferCacheEntry() = default;

size_t ServiceShaderTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceShaderTransferCacheEntry::Deserialize(
    GrContext* context,
    base::span<const uint8_t> data) {
  // These entries must be created directly via CreateLocalEntry.
  NOTREACHED();
  return false;
}

}  // namespace cc

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/transfer_cache_entry.h"

#include <memory>

#include "base/notreached.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/paint/shader_transfer_cache_entry.h"
#include "cc/paint/skottie_transfer_cache_entry.h"

namespace cc {

std::unique_ptr<ServiceTransferCacheEntry> ServiceTransferCacheEntry::Create(
    TransferCacheEntryType type) {
  switch (type) {
    case TransferCacheEntryType::kRawMemory:
      return std::make_unique<ServiceRawMemoryTransferCacheEntry>();
    case TransferCacheEntryType::kImage:
      return std::make_unique<ServiceImageTransferCacheEntry>();
    case TransferCacheEntryType::kShader:
      // ServiceShader/TextBlobTransferCache is only created via
      // CreateLocalEntry and is never serialized/deserialized.
      return nullptr;
    case TransferCacheEntryType::kSkottie:
      return std::make_unique<ServiceSkottieTransferCacheEntry>();
  }

  return nullptr;
}

bool ServiceTransferCacheEntry::SafeConvertToType(
    uint32_t raw_type,
    TransferCacheEntryType* type) {
  if (raw_type > static_cast<uint32_t>(TransferCacheEntryType::kLast))
    return false;

  *type = static_cast<TransferCacheEntryType>(raw_type);
  return true;
}

// static
bool ServiceTransferCacheEntry::UsesGpuContext(TransferCacheEntryType type) {
  switch (type) {
    case TransferCacheEntryType::kRawMemory:
    case TransferCacheEntryType::kShader:
    case TransferCacheEntryType::kSkottie:
      return false;
    case TransferCacheEntryType::kImage:
      return true;
  }

  NOTREACHED();
}

}  // namespace cc

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_
#define CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_

#include <cstdint>

#include <memory>

#include "cc/paint/paint_export.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

// Helper interface consumed by cc/paint during OOP raster deserialization.
// Provides access to the transfer cache.
// TODO(ericrk): We should use TransferCacheEntryId, not uint64_t here, but
// we need to figure out layering. crbug.com/777622
class CC_PAINT_EXPORT TransferCacheDeserializeHelper {
 public:
  virtual ~TransferCacheDeserializeHelper() = default;

  // Type safe access to an entry in the transfer cache. Returns null if the
  // entry is missing or of the wrong type.
  template <typename T>
  T* GetEntryAs(uint32_t id) {
    // There is a bit of a weirdness if we use T::kType directly in the DCHECK
    // below. Specifically, the linker can't seem to find that symbol ¯\_(ツ)_/¯
    // so instead save off the type into a local variable and use that.
    auto entry_type = T::kType;
    ServiceTransferCacheEntry* entry = GetEntryInternal(entry_type, id);
    if (entry == nullptr) {
      return nullptr;
    }

    total_size_ += entry->CachedSize();

    // The service side entry is created using T::kType, so the class created is
    // guaranteed to make the entry type.
    DCHECK_EQ(entry->Type(), entry_type);
    return static_cast<T*>(entry);
  }

  // Creates an entry directly.  If an entry exists, it will be clobbered.
  virtual void CreateLocalEntry(
      uint32_t id,
      std::unique_ptr<ServiceTransferCacheEntry> entry) = 0;

  size_t GetTotalEntrySizes() const { return total_size_; }

 private:
  virtual ServiceTransferCacheEntry* GetEntryInternal(
      TransferCacheEntryType entry_type,
      uint32_t entry_id) = 0;

  size_t total_size_ = 0;
};

}  // namespace cc

#endif  // CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_

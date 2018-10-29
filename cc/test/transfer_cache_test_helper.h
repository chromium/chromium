// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_
#define CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_

#include <map>
#include <vector>

#include "base/containers/span.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "third_party/skia/include/gpu/GrContext.h"

class GrContext;

namespace cc {

class TransferCacheTestHelper : public TransferCacheDeserializeHelper,
                                public TransferCacheSerializeHelper {
 public:
  explicit TransferCacheTestHelper(GrContext* context = nullptr);
  ~TransferCacheTestHelper() override;

  void SetGrContext(GrContext* context);
  void SetCachedItemsLimit(size_t limit);

  // Direct Access API (simulates ContextSupport methods).
  bool LockEntryDirect(const EntryKey& key);

  void CreateEntryDirect(const EntryKey& key, base::span<uint8_t> data);
  void UnlockEntriesDirect(const std::vector<EntryKey>& keys);
  void DeleteEntryDirect(const EntryKey& key);

  // Deserialization helpers.
  ServiceTransferCacheEntry* GetEntryInternal(TransferCacheEntryType type,
                                              uint32_t id) override;

  const EntryKey& GetLastAddedEntry() const { return last_added_entry_; }

  void CreateLocalEntry(
      uint32_t id,
      std::unique_ptr<ServiceTransferCacheEntry> entry) override;

 protected:
  // Serialization helpers.
  bool LockEntryInternal(const EntryKey& key) override;
  size_t CreateEntryInternal(const ClientTransferCacheEntry& entry,
                             char* memory) override;
  void FlushEntriesInternal(std::set<EntryKey> keys) override;

 private:
  // Helper functions.
  void EnforceLimits();

  std::map<EntryKey, std::unique_ptr<ServiceTransferCacheEntry>> entries_;
  std::set<EntryKey> local_entries_;
  std::set<EntryKey> locked_entries_;
  EntryKey last_added_entry_ = {TransferCacheEntryType::kRawMemory, ~0};

  GrContext* context_ = nullptr;
  sk_sp<GrContext> owned_context_;
  size_t cached_items_limit_ = std::numeric_limits<size_t>::max();
};

}  // namespace cc

#endif  // CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_
#define CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/transfer_cache_deserialize_helper.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace cc {

class TransferCacheTestHelper : public TransferCacheDeserializeHelper,
                                public TransferCacheSerializeHelper {
 public:
  // Entries created locally by the service side during deserialization (e.g.
  // record shader entries) are keyed by 64-bit ids, while client entries use
  // 32-bit ids.
  using ServiceEntryKey = std::pair<TransferCacheEntryType, uint64_t>;

  TransferCacheTestHelper();
  ~TransferCacheTestHelper() override;

  void SetCachedItemsLimit(size_t limit);

  // Direct Access API (simulates ContextSupport methods).
  bool LockEntryDirect(const EntryKey& key);

  void CreateEntryDirect(const EntryKey& key, base::span<uint8_t> data);
  void UnlockEntriesDirect(const std::vector<EntryKey>& keys);
  void DeleteEntryDirect(const ServiceEntryKey& key);

  // Deserialization helpers.
  ServiceTransferCacheEntry* GetEntryInternal(TransferCacheEntryType type,
                                              uint64_t id) override;

  const ServiceEntryKey& GetLastAddedEntry() const { return last_added_entry_; }

  void CreateLocalEntry(
      uint64_t id,
      std::unique_ptr<ServiceTransferCacheEntry> entry) override;

  size_t num_of_entries() const { return entries_.size(); }

 protected:
  // Serialization helpers.
  bool LockEntryInternal(const EntryKey& key) override;
  uint32_t CreateEntryInternal(const ClientTransferCacheEntry& entry,
                               base::span<uint8_t> memory) override;
  void FlushEntriesInternal(std::set<EntryKey> keys) override;

 private:
  // Helper functions.
  void EnforceLimits();

  sk_sp<GrDirectContext> context_;

  // entries_ may reference owned_context_ so must be destroyed before the
  // context to avoid dangling ptrs.
  std::map<ServiceEntryKey, std::unique_ptr<ServiceTransferCacheEntry>>
      entries_;
  std::set<ServiceEntryKey> local_entries_;
  std::set<ServiceEntryKey> locked_entries_;
  ServiceEntryKey last_added_entry_ = {TransferCacheEntryType::kRawMemory, ~0};

  size_t cached_items_limit_ = std::numeric_limits<size_t>::max();
};

}  // namespace cc

#endif  // CC_TEST_TRANSFER_CACHE_TEST_HELPER_H_

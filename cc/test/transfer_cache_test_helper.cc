// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/transfer_cache_test_helper.h"

#include "base/containers/span.h"
#include "base/logging.h"

namespace cc {

TransferCacheTestHelper::TransferCacheTestHelper(GrContext* context)
    : context_(context) {
  if (!context_) {
    owned_context_ = GrContext::MakeMock(nullptr);
    context_ = owned_context_.get();
  }
}
TransferCacheTestHelper::~TransferCacheTestHelper() = default;

bool TransferCacheTestHelper::LockEntryDirect(const EntryKey& key) {
  return LockEntryInternal(key);
}

void TransferCacheTestHelper::CreateEntryDirect(const EntryKey& key,
                                                base::span<uint8_t> data) {
  // Deserialize into a service transfer cache entry.
  std::unique_ptr<ServiceTransferCacheEntry> service_entry =
      ServiceTransferCacheEntry::Create(key.first);
  if (!service_entry)
    return;

  bool success = service_entry->Deserialize(context_, data);
  if (!success)
    return;

  last_added_entry_ = key;

  // Put things into the cache.
  entries_.emplace(key, std::move(service_entry));
  locked_entries_.insert(key);
  EnforceLimits();
}

void TransferCacheTestHelper::CreateLocalEntry(
    uint32_t id,
    std::unique_ptr<ServiceTransferCacheEntry> entry) {
  auto key = std::make_pair(entry->Type(), id);

  DeleteEntryDirect(key);

  entries_[key] = std::move(entry);
  local_entries_.insert(key);
  last_added_entry_ = key;
}

void TransferCacheTestHelper::UnlockEntriesDirect(
    const std::vector<EntryKey>& keys) {
  for (const auto& key : keys) {
    locked_entries_.erase(key);
  }
  EnforceLimits();
}

void TransferCacheTestHelper::DeleteEntryDirect(const EntryKey& key) {
  locked_entries_.erase(key);
  local_entries_.erase(key);
  entries_.erase(key);
}

void TransferCacheTestHelper::SetGrContext(GrContext* context) {
  context_ = context;
}

void TransferCacheTestHelper::SetCachedItemsLimit(size_t limit) {
  cached_items_limit_ = limit;
  EnforceLimits();
}

ServiceTransferCacheEntry* TransferCacheTestHelper::GetEntryInternal(
    TransferCacheEntryType type,
    uint32_t id) {
  auto key = std::make_pair(type, id);
  if (locked_entries_.count(key) + local_entries_.count(key) == 0)
    return nullptr;
  if (entries_.find(key) == entries_.end())
    return nullptr;
  return entries_[key].get();
}

bool TransferCacheTestHelper::LockEntryInternal(const EntryKey& key) {
  if (entries_.find(key) == entries_.end())
    return false;

  locked_entries_.insert(key);
  EnforceLimits();
  return true;
}

uint32_t TransferCacheTestHelper::CreateEntryInternal(
    const ClientTransferCacheEntry& client_entry,
    char* memory) {
  auto key = std::make_pair(client_entry.Type(), client_entry.Id());
  DCHECK(entries_.find(key) == entries_.end());

  // Serialize data.
  uint32_t size = client_entry.SerializedSize();
  std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
  auto span = base::make_span(data.get(), size);
  bool success = client_entry.Serialize(span);
  DCHECK(success);
  CreateEntryDirect(key, span);
  return 0u;
}

void TransferCacheTestHelper::FlushEntriesInternal(std::set<EntryKey> keys) {
  for (auto& key : keys)
    locked_entries_.erase(key);
  EnforceLimits();
}

void TransferCacheTestHelper::EnforceLimits() {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (entries_.size() <= cached_items_limit_)
      break;

    auto found = locked_entries_.find(it->first);
    if (found == locked_entries_.end()) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace cc

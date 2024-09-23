// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/transfer_cache_serialize_helper.h"

#include <utility>

#include "base/check_op.h"

namespace cc {

TransferCacheSerializeHelper::TransferCacheSerializeHelper() = default;
TransferCacheSerializeHelper::~TransferCacheSerializeHelper() = default;

bool TransferCacheSerializeHelper::LockEntry(TransferCacheEntryType type,
                                             uint32_t id) {
  EntryKey key(type, id);
  // Entry already locked, so we don't need to process it.
  if (added_entries_.count(key) != 0)
    return true;

  bool success = LockEntryInternal(key);
  if (!success)
    return false;
  added_entries_.insert(key);
  return true;
}

uint32_t TransferCacheSerializeHelper::CreateEntry(
    const ClientTransferCacheEntry& entry,
    uint8_t* memory) {
  // We shouldn't be creating entries if they were already created or locked.
  EntryKey key(entry.Type(), entry.Id());
  DCHECK_EQ(added_entries_.count(key), 0u);
  added_entries_.insert(key);
  return CreateEntryInternal(entry, memory);
}

void TransferCacheSerializeHelper::FlushEntries() {
  FlushEntriesInternal(std::move(added_entries_));
  added_entries_.clear();
}

void TransferCacheSerializeHelper::AssertLocked(TransferCacheEntryType type,
                                                uint32_t id) {
  DCHECK_EQ(added_entries_.count(EntryKey(type, id)), 1u);
}

}  // namespace cc

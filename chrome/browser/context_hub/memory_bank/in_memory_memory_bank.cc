// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "base/time/time.h"

namespace context_hub {

namespace {
constexpr size_t kMaxEntries = 50;
}  // namespace

InMemoryMemoryBank::InMemoryMemoryBank() : entries_(kMaxEntries) {}
InMemoryMemoryBank::~InMemoryMemoryBank() = default;

void InMemoryMemoryBank::SaveMemoryBankEntry(
    MemoryBankEntry entry,
    OperationCompleteCallback callback) {
  if (entry.id == 0) {
    entry.id = static_cast<int64_t>(
        base::RandGenerator(std::numeric_limits<int64_t>::max()));
  }
  if (entry.timestamp.is_null()) {
    entry.timestamp = base::Time::Now();
  }
  int64_t entry_id = entry.id;
  entries_.Put(entry_id, std::move(entry));
  if (callback) {
    std::move(callback).Run(/*success=*/true);
  }
}

void InMemoryMemoryBank::GetAllEntries(GetEntriesCallback callback) const {
  std::vector<MemoryBankEntry> result;
  for (const auto& [id, entry] : entries_) {
    result.push_back(entry);
  }
  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

void InMemoryMemoryBank::GetEntriesByIds(base::span<const int64_t> ids,
                                         GetEntriesCallback callback) const {
  std::vector<MemoryBankEntry> result;
  for (int64_t id : ids) {
    auto it = entries_.Peek(id);
    if (it != entries_.end()) {
      result.push_back(it->second);
    }
  }
  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

void InMemoryMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) {
  for (int64_t id : ids) {
    auto it = entries_.Peek(id);
    if (it != entries_.end()) {
      entries_.Erase(it);
    }
  }
  if (callback) {
    std::move(callback).Run(/*success=*/true);
  }
}

}  // namespace context_hub

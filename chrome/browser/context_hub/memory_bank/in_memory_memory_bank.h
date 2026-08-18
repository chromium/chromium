// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_

#include <cstdint>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

class InMemoryMemoryBank : public MemoryBank {
 public:
  InMemoryMemoryBank();
  InMemoryMemoryBank(const InMemoryMemoryBank&) = delete;
  InMemoryMemoryBank& operator=(const InMemoryMemoryBank&) = delete;
  ~InMemoryMemoryBank() override;

  // MemoryBank:
  void SaveMemoryBankEntry(MemoryBankEntry entry,
                           OperationCompleteCallback callback) override;
  void GetAllEntries(GetEntriesCallback callback) const override;
  void GetEntriesByIds(base::span<const int64_t> ids,
                       GetEntriesCallback callback) const override;
  void DeleteEntries(base::span<const int64_t> ids,
                     OperationCompleteCallback callback) override;

 private:
  // LRU cache to store the entries in the memory bank.
  base::LRUCache<int64_t, MemoryBankEntry> entries_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_

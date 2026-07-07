// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_

#include <optional>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

class InMemoryMemoryBank : public MemoryBank {
 public:
  InMemoryMemoryBank();
  ~InMemoryMemoryBank() override;

  InMemoryMemoryBank(const InMemoryMemoryBank&) = delete;
  InMemoryMemoryBank& operator=(const InMemoryMemoryBank&) = delete;

  // MemoryBank:
  void SaveTab(const GURL& url,
               const std::string& tab_title,
               const std::string& page_text,
               OperationCompleteCallback callback) override;
  void SaveTextSelection(const GURL& url,
                         const std::string& tab_title,
                         const std::string& selected_text,
                         OperationCompleteCallback callback) override;
  void GetAllEntries(GetAllEntriesCallback callback) const override;
  void DeleteEntries(base::span<const int64_t> ids,
                     OperationCompleteCallback callback) override;

 private:
  // LRU cache to store the entries in the memory bank.
  base::LRUCache<int64_t, MemoryBankEntry> entries_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_IN_MEMORY_MEMORY_BANK_H_

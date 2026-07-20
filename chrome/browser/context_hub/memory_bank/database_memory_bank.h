// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"

namespace context_hub {

class ContextHubBackend;

// Database-backed implementation of MemoryBank.
// Delegates memory bank operations to ContextHubBackend.
class DatabaseMemoryBank : public MemoryBank {
 public:
  explicit DatabaseMemoryBank(ContextHubBackend& context_hub_backend);
  DatabaseMemoryBank(const DatabaseMemoryBank&) = delete;
  DatabaseMemoryBank& operator=(const DatabaseMemoryBank&) = delete;
  ~DatabaseMemoryBank() override;

  // MemoryBank implementation:
  void SaveTab(const GURL& url,
               const std::string& tab_title,
               const std::string& page_text,
               OperationCompleteCallback callback) override;
  void SaveTextSelection(const GURL& url,
                         const std::string& tab_title,
                         const std::string& selected_text,
                         OperationCompleteCallback callback) override;
  void DeleteEntries(base::span<const int64_t> ids,
                     OperationCompleteCallback callback) override;
  void GetAllEntries(GetAllEntriesCallback callback) const override;

 private:
  const raw_ref<ContextHubBackend> context_hub_backend_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_

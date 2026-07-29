// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

class GURL;

namespace context_hub {

class MemoryBank {
 public:
  virtual ~MemoryBank() = default;

  using OperationCompleteCallback = base::OnceClosure;
  // Saves a tab to the memory bank.
  virtual void SaveTab(const GURL& url,
                       std::string_view tab_title,
                       std::string_view page_text,
                       OperationCompleteCallback callback) = 0;
  // Saves a text selection to the memory bank.
  virtual void SaveTextSelection(const GURL& url,
                                 std::string_view tab_title,
                                 std::string_view selected_text,
                                 OperationCompleteCallback callback) = 0;
  // Deletes entries from the memory bank.
  virtual void DeleteEntries(base::span<const int64_t> ids,
                             OperationCompleteCallback callback) = 0;
  using GetEntriesCallback =
      base::OnceCallback<void(std::vector<MemoryBankEntry>)>;
  // Returns all entries from the memory bank via the callback.
  virtual void GetAllEntries(GetEntriesCallback callback) const = 0;
  // Returns entries for the given IDs from the memory bank via the callback.
  virtual void GetEntriesByIds(base::span<const int64_t> ids,
                               GetEntriesCallback callback) const = 0;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_

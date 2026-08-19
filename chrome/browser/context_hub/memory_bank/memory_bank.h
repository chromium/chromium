// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

class MemoryBank {
 public:
  virtual ~MemoryBank() = default;

  using OperationCompleteCallback = base::OnceCallback<void(bool)>;
  // Saves or updates an entry in the memory bank.
  virtual void SaveMemoryBankEntry(MemoryBankEntry entry,
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

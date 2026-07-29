// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_H_
#define CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"

namespace context_hub {

struct MemoryBankEntry;

// Interface for ContextHub central database storage.
// Provides data access for all ContextHub tables/stores.
class ContextHubBackend {
 public:
  virtual ~ContextHubBackend() = default;

  // MemoryBankTable operations:
  using OperationCompleteCallback = base::OnceCallback<void(bool)>;
  // Adds or updates an entry in the MemoryBankTable.
  virtual void AddOrUpdateMemoryBankEntry(
      MemoryBankEntry entry,
      OperationCompleteCallback callback) = 0;
  // Deletes entries in the MemoryBankTable with the given ids.
  virtual void DeleteMemoryBankEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) = 0;
  using GetEntriesCallback =
      base::OnceCallback<void(std::vector<MemoryBankEntry>)>;
  // Returns all entries in the MemoryBankTable.
  virtual void GetAllMemoryBankEntries(
      GetEntriesCallback callback) const = 0;
  // Returns entries for the given IDs in the MemoryBankTable.
  virtual void GetMemoryBankEntriesByIds(
      base::span<const int64_t> ids,
      GetEntriesCallback callback) const = 0;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_STORAGE_CONTEXT_HUB_BACKEND_H_

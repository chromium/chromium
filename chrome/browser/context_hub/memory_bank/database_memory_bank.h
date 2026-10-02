// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SaveMemoryBankEntry(MemoryBankEntry entry,
                           OperationCompleteCallback callback) override;
  void UpdateEntryAnnotations(int64_t id,
                              std::vector<std::string> tags,
                              std::optional<std::string> note,
                              std::optional<std::string> collection,
                              OperationCompleteCallback callback) override;
  void DeleteEntries(base::span<const int64_t> ids,
                     OperationCompleteCallback callback) override;
  void GetAllEntries(GetEntriesCallback callback) const override;
  void GetEntriesByIds(base::span<const int64_t> ids,
                       GetEntriesCallback callback) const override;
  void GetAllTags(GetStringsCallback callback) const override;
  void GetAllCollections(GetStringsCallback callback) const override;

 private:
  void OnEntrySaved(MemoryBankEntry entry,
                    OperationCompleteCallback callback,
                    std::optional<int64_t> id);
  void OnEntryAnnotationsUpdated(int64_t id,
                                 const std::vector<std::string>& tags,
                                 const std::optional<std::string>& note,
                                 const std::optional<std::string>& collection,
                                 OperationCompleteCallback callback,
                                 bool success);
  void OnEntriesDeleted(const std::vector<int64_t>& ids,
                        OperationCompleteCallback callback,
                        bool success);

  const raw_ref<ContextHubBackend> context_hub_backend_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<DatabaseMemoryBank> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_DATABASE_MEMORY_BANK_H_

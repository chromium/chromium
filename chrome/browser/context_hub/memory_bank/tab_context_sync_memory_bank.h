// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_TAB_CONTEXT_SYNC_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_TAB_CONTEXT_SYNC_MEMORY_BANK_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "components/sync_tab_context/container_id.h"

class PrefService;

namespace sync_tab_context {
class TabContextSyncService;
}  // namespace sync_tab_context

namespace context_hub {

class TabContextSyncMemoryBank : public MemoryBank {
 public:
  TabContextSyncMemoryBank(
      PrefService* pref_service,
      sync_tab_context::TabContextSyncService& tab_context_sync_service);
  TabContextSyncMemoryBank(const TabContextSyncMemoryBank&) = delete;
  TabContextSyncMemoryBank& operator=(const TabContextSyncMemoryBank&) = delete;
  ~TabContextSyncMemoryBank() override;

  // MemoryBank implementation:
  void SaveMemoryBankEntry(MemoryBankEntry entry,
                           OperationCompleteCallback callback) override;
  void DeleteEntries(base::span<const int64_t> ids,
                     OperationCompleteCallback callback) override;
  void GetAllEntries(GetEntriesCallback callback) const override;
  void GetEntriesByIds(base::span<const int64_t> ids,
                       GetEntriesCallback callback) const override;

 private:
  std::optional<sync_tab_context::ContainerId> GetOrCreateContainerId();

  const raw_ptr<PrefService> pref_service_;
  const raw_ref<sync_tab_context::TabContextSyncService>
      tab_context_sync_service_;

  std::optional<sync_tab_context::ContainerId> cached_container_id_;
  mutable base::LRUCache<int64_t, MemoryBankEntry> entries_;

  base::WeakPtrFactory<TabContextSyncMemoryBank> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_TAB_CONTEXT_SYNC_MEMORY_BANK_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/database_memory_bank.h"

#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"

namespace context_hub {

DatabaseMemoryBank::DatabaseMemoryBank(ContextHubBackend& context_hub_backend)
    : context_hub_backend_(context_hub_backend) {}

DatabaseMemoryBank::~DatabaseMemoryBank() = default;

void DatabaseMemoryBank::SaveMemoryBankEntry(
    MemoryBankEntry entry,
    OperationCompleteCallback callback) {
  if (entry.timestamp.is_null()) {
    entry.timestamp = base::Time::Now();
  }
  context_hub_backend_->AddOrUpdateMemoryBankEntry(std::move(entry),
                                                   std::move(callback));
}

void DatabaseMemoryBank::UpdateEntryAnnotations(
    int64_t id,
    std::vector<std::string> tags,
    std::optional<std::string> note,
    std::optional<std::string> collection,
    OperationCompleteCallback callback) {
  context_hub_backend_->UpdateMemoryBankEntryAnnotations(
      id, std::move(tags), std::move(note), std::move(collection),
      std::move(callback));
}

void DatabaseMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) {
  context_hub_backend_->DeleteMemoryBankEntries(ids, std::move(callback));
}

void DatabaseMemoryBank::GetAllEntries(GetEntriesCallback callback) const {
  context_hub_backend_->GetAllMemoryBankEntries(std::move(callback));
}

void DatabaseMemoryBank::GetEntriesByIds(base::span<const int64_t> ids,
                                         GetEntriesCallback callback) const {
  context_hub_backend_->GetMemoryBankEntriesByIds(ids, std::move(callback));
}

void DatabaseMemoryBank::GetAllTags(GetStringsCallback callback) const {
  context_hub_backend_->GetAllMemoryBankTags(std::move(callback));
}

void DatabaseMemoryBank::GetAllCollections(GetStringsCallback callback) const {
  context_hub_backend_->GetAllMemoryBankCollections(std::move(callback));
}

}  // namespace context_hub

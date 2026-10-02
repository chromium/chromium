// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/database_memory_bank.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"

namespace context_hub {

DatabaseMemoryBank::DatabaseMemoryBank(ContextHubBackend& context_hub_backend)
    : context_hub_backend_(context_hub_backend) {}

DatabaseMemoryBank::~DatabaseMemoryBank() = default;

void DatabaseMemoryBank::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DatabaseMemoryBank::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DatabaseMemoryBank::SaveMemoryBankEntry(
    MemoryBankEntry entry,
    OperationCompleteCallback callback) {
  if (entry.timestamp.is_null()) {
    entry.timestamp = base::Time::Now();
  }
  MemoryBankEntry saved_entry = entry;
  context_hub_backend_->AddOrUpdateMemoryBankEntry(
      std::move(entry),
      base::BindOnce(&DatabaseMemoryBank::OnEntrySaved,
                     weak_factory_.GetWeakPtr(), std::move(saved_entry),
                     std::move(callback)));
}

void DatabaseMemoryBank::UpdateEntryAnnotations(
    int64_t id,
    std::vector<std::string> tags,
    std::optional<std::string> note,
    std::optional<std::string> collection,
    OperationCompleteCallback callback) {
  std::vector<std::string> updated_tags = tags;
  std::optional<std::string> updated_note = note;
  std::optional<std::string> updated_collection = collection;
  context_hub_backend_->UpdateMemoryBankEntryAnnotations(
      id, std::move(tags), std::move(note), std::move(collection),
      base::BindOnce(&DatabaseMemoryBank::OnEntryAnnotationsUpdated,
                     weak_factory_.GetWeakPtr(), id, std::move(updated_tags),
                     std::move(updated_note), std::move(updated_collection),
                     std::move(callback)));
}

void DatabaseMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) {
  std::vector<int64_t> requested_ids = base::ToVector(ids);
  context_hub_backend_->DeleteMemoryBankEntries(
      ids, base::BindOnce(&DatabaseMemoryBank::OnEntriesDeleted,
                          weak_factory_.GetWeakPtr(), std::move(requested_ids),
                          std::move(callback)));
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

void DatabaseMemoryBank::OnEntrySaved(MemoryBankEntry entry,
                                      OperationCompleteCallback callback,
                                      std::optional<int64_t> id) {
  if (!id.has_value()) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }
  entry.id = *id;
  observers_.Notify(&Observer::OnMemoryBankEntryAdded, entry);
  if (callback) {
    std::move(callback).Run(true);
  }
}

void DatabaseMemoryBank::OnEntryAnnotationsUpdated(
    int64_t id,
    const std::vector<std::string>& tags,
    const std::optional<std::string>& note,
    const std::optional<std::string>& collection,
    OperationCompleteCallback callback,
    bool success) {
  if (success) {
    observers_.Notify(&Observer::OnMemoryBankEntryUpdated, id, tags, note,
                      collection);
  }
  if (callback) {
    std::move(callback).Run(success);
  }
}

void DatabaseMemoryBank::OnEntriesDeleted(const std::vector<int64_t>& ids,
                                          OperationCompleteCallback callback,
                                          bool success) {
  if (success && !ids.empty()) {
    observers_.Notify(&Observer::OnMemoryBankEntriesDeleted, ids);
  }
  if (callback) {
    std::move(callback).Run(success);
  }
}

}  // namespace context_hub

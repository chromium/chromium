// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/tab_context_sync_memory_bank.h"

#include <limits>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/context_hub/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync_tab_context/tab_context_sync_service.h"

namespace context_hub {

namespace {
constexpr size_t kMaxDebugEntries = 100;
}  // namespace

TabContextSyncMemoryBank::TabContextSyncMemoryBank(
    PrefService* pref_service,
    sync_tab_context::TabContextSyncService& tab_context_sync_service)
    : pref_service_(pref_service),
      tab_context_sync_service_(tab_context_sync_service),
      entries_(kMaxDebugEntries) {}

TabContextSyncMemoryBank::~TabContextSyncMemoryBank() = default;

std::optional<sync_tab_context::ContainerId>
TabContextSyncMemoryBank::GetOrCreateContainerId() {
  if (cached_container_id_.has_value()) {
    return *cached_container_id_;
  }

  if (pref_service_) {
    const std::string pref_container_id =
        pref_service_->GetString(prefs::kContextHubTabContextSyncContainerId);
    if (!pref_container_id.empty()) {
      base::Uuid uuid = base::Uuid::ParseCaseInsensitive(pref_container_id);
      if (uuid.is_valid()) {
        cached_container_id_ = sync_tab_context::ContainerId(std::move(uuid));
        return *cached_container_id_;
      }
    }
  }

  std::optional<sync_tab_context::ContainerId> new_container_id =
      tab_context_sync_service_->CreateContainer();
  if (!new_container_id.has_value()) {
    LOG(ERROR) << "Failed to create container via TabContextSyncService.";
    return std::nullopt;
  }

  cached_container_id_ = new_container_id;
  if (pref_service_) {
    pref_service_->SetString(prefs::kContextHubTabContextSyncContainerId,
                             new_container_id->value().AsLowercaseString());
  }

  return *cached_container_id_;
}

void TabContextSyncMemoryBank::SaveMemoryBankEntry(
    MemoryBankEntry entry,
    OperationCompleteCallback callback) {
  if (entry.id == 0) {
    entry.id = static_cast<int64_t>(
        base::RandGenerator(std::numeric_limits<int64_t>::max()));
  }
  if (entry.timestamp.is_null()) {
    entry.timestamp = base::Time::Now();
  }

  std::optional<sync_tab_context::ContainerId> container_id =
      GetOrCreateContainerId();
  if (!container_id.has_value()) {
    LOG(WARNING) << "Container ID unavailable. Aborting SaveMemoryBankEntry.";
    if (callback) {
      std::move(callback).Run(/*success=*/false);
    }
    return;
  }

  const int64_t entry_id = entry.id;
  std::string payload = entry.selected_text.value_or(std::string());

  entries_.Put(entry_id, std::move(entry));

  bool upload_success = tab_context_sync_service_->UploadPageContext(
      *container_id, base::NumberToString(entry_id), std::move(payload));

  if (callback) {
    std::move(callback).Run(upload_success);
  }
}

void TabContextSyncMemoryBank::GetAllEntries(
    GetEntriesCallback callback) const {
  std::vector<MemoryBankEntry> results;
  for (const auto& [id, entry] : entries_) {
    results.push_back(entry);
  }
  if (callback) {
    std::move(callback).Run(std::move(results));
  }
}

void TabContextSyncMemoryBank::GetEntriesByIds(
    base::span<const int64_t> ids,
    GetEntriesCallback callback) const {
  std::vector<MemoryBankEntry> results;
  for (int64_t id : ids) {
    auto it = entries_.Peek(id);
    if (it != entries_.end()) {
      results.push_back(it->second);
    }
  }
  if (callback) {
    std::move(callback).Run(std::move(results));
  }
}

void TabContextSyncMemoryBank::DeleteEntries(
    base::span<const int64_t> ids,
    OperationCompleteCallback callback) {
  for (int64_t id : ids) {
    auto it = entries_.Peek(id);
    if (it != entries_.end()) {
      entries_.Erase(it);
    }
  }
  if (callback) {
    std::move(callback).Run(/*success=*/true);
  }
}

}  // namespace context_hub

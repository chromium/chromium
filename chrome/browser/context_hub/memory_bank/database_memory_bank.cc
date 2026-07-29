// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/database_memory_bank.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "url/gurl.h"

namespace context_hub {

DatabaseMemoryBank::DatabaseMemoryBank(ContextHubBackend& context_hub_backend)
    : context_hub_backend_(context_hub_backend) {}

DatabaseMemoryBank::~DatabaseMemoryBank() = default;

void DatabaseMemoryBank::SaveTab(const GURL& url,
                                 std::string_view tab_title,
                                 std::string_view page_text,
                                 OperationCompleteCallback callback) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::Now();
  entry.url = url;
  entry.tab_title = std::string(tab_title);
  // TODO(crbug.com/530253460): Reconsider whether we should save an entry if
  // page_text is empty.
  if (!page_text.empty()) {
    entry.selected_text = std::string(page_text);
  }

  // TODO(crbug.com/534780677): Use the return value of
  // AddOrUpdateMemoryBankEntry.
  context_hub_backend_->AddOrUpdateMemoryBankEntry(
      std::move(entry), base::IgnoreArgs<bool>(std::move(callback)));
}

void DatabaseMemoryBank::SaveTextSelection(const GURL& url,
                                           std::string_view tab_title,
                                           std::string_view selected_text,
                                           OperationCompleteCallback callback) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::Now();
  entry.url = url;
  entry.tab_title = std::string(tab_title);
  if (!selected_text.empty()) {
    entry.selected_text = std::string(selected_text);
  }

  context_hub_backend_->AddOrUpdateMemoryBankEntry(
      std::move(entry), base::IgnoreArgs<bool>(std::move(callback)));
}

void DatabaseMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) {
  context_hub_backend_->DeleteMemoryBankEntries(
      ids, base::IgnoreArgs<bool>(std::move(callback)));
}

void DatabaseMemoryBank::GetAllEntries(GetEntriesCallback callback) const {
  context_hub_backend_->GetAllMemoryBankEntries(std::move(callback));
}

void DatabaseMemoryBank::GetEntriesByIds(base::span<const int64_t> ids,
                                         GetEntriesCallback callback) const {
  context_hub_backend_->GetMemoryBankEntriesByIds(ids, std::move(callback));
}

}  // namespace context_hub

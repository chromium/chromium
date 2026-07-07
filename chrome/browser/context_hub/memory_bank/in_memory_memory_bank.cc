// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"

#include <limits>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace context_hub {

namespace {
constexpr size_t kMaxEntries = 50;
}  // namespace

InMemoryMemoryBank::InMemoryMemoryBank() : entries_(kMaxEntries) {}
InMemoryMemoryBank::~InMemoryMemoryBank() = default;

void InMemoryMemoryBank::SaveTab(const GURL& url,
                                 const std::string& tab_title,
                                 const std::string& page_text,
                                 OperationCompleteCallback callback) {
  MemoryBankEntry entry;
  entry.id = static_cast<int64_t>(
      base::RandGenerator(std::numeric_limits<int64_t>::max()));
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::Now();
  entry.url = url;
  entry.tab_title = tab_title;
  entry.selected_text = page_text;
  entries_.Put(entry.id, std::move(entry));
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryMemoryBank::SaveTextSelection(const GURL& url,
                                           const std::string& tab_title,
                                           const std::string& selected_text,
                                           OperationCompleteCallback callback) {
  MemoryBankEntry entry;
  entry.id = static_cast<int64_t>(
      base::RandGenerator(std::numeric_limits<int64_t>::max()));
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::Now();
  entry.url = url;
  entry.tab_title = tab_title;
  entry.selected_text = selected_text;
  entries_.Put(entry.id, std::move(entry));
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryMemoryBank::GetAllEntries(GetAllEntriesCallback callback) const {
  std::vector<MemoryBankEntry> result;
  for (const auto& [id, entry] : entries_) {
    result.push_back(entry);
  }
  if (callback) {
  std::move(callback).Run(std::move(result));
  }
}

void InMemoryMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                       OperationCompleteCallback callback) {
  for (int64_t id : ids) {
    auto it = entries_.Peek(id);
    if (it != entries_.end()) {
      entries_.Erase(it);
    }
  }
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace context_hub

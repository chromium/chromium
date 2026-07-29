// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"

#include <string_view>
#include <vector>

#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

NoOpMemoryBank::NoOpMemoryBank() = default;
NoOpMemoryBank::~NoOpMemoryBank() = default;

void NoOpMemoryBank::SaveTab(const GURL& url,
                             std::string_view tab_title,
                             std::string_view page_text,
                             OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

void NoOpMemoryBank::SaveTextSelection(const GURL& url,
                                       std::string_view tab_title,
                                       std::string_view selected_text,
                                       OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

void NoOpMemoryBank::GetAllEntries(GetEntriesCallback callback) const {
  std::move(callback).Run({});
}

void NoOpMemoryBank::GetEntriesByIds(base::span<const int64_t> ids,
                                     GetEntriesCallback callback) const {
  std::move(callback).Run({});
}

void NoOpMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                   OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace context_hub

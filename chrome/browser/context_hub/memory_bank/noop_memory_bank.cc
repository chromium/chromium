// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"

#include <vector>

#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

NoOpMemoryBank::NoOpMemoryBank() = default;
NoOpMemoryBank::~NoOpMemoryBank() = default;

void NoOpMemoryBank::SaveTab(const GURL& url,
                             const std::string& tab_title,
                             const std::string& page_text,
                             OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

void NoOpMemoryBank::SaveTextSelection(const GURL& url,
                                       const std::string& tab_title,
                                       const std::string& selected_text,
                                       OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

void NoOpMemoryBank::GetAllEntries(GetAllEntriesCallback callback) const {
  std::move(callback).Run({});
}

void NoOpMemoryBank::DeleteEntries(base::span<const int64_t> ids,
                                   OperationCompleteCallback callback) {
  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace context_hub

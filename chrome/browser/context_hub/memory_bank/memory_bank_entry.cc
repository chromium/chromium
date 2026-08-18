// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

#include <optional>
#include <string>
#include <utility>

#include "url/gurl.h"

namespace context_hub {

MemoryBankEntry::MemoryBankEntry() = default;

MemoryBankEntry::MemoryBankEntry(MemoryBankType type,
                                 GURL url,
                                 std::string tab_title,
                                 std::optional<std::string> selected_text)
    : type(type),
      url(std::move(url)),
      tab_title(std::move(tab_title)),
      selected_text(std::move(selected_text)) {}

MemoryBankEntry::MemoryBankEntry(const MemoryBankEntry&) = default;
MemoryBankEntry& MemoryBankEntry::operator=(const MemoryBankEntry&) = default;
MemoryBankEntry::MemoryBankEntry(MemoryBankEntry&&) = default;
MemoryBankEntry& MemoryBankEntry::operator=(MemoryBankEntry&&) = default;
MemoryBankEntry::~MemoryBankEntry() = default;

}  // namespace context_hub

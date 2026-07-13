// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_ENTRY_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_ENTRY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace context_hub {

enum class MemoryBankType { kTab, kTextSelection };

struct MemoryBankEntry {
  int64_t id = 0;
  MemoryBankType type = MemoryBankType::kTab;
  base::Time timestamp;
  GURL url;
  std::string tab_title;
  std::optional<std::string> selected_text;
  // Tags associated with the entry for grouping.
  std::vector<std::string> tags;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_ENTRY_H_

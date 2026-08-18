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
  MemoryBankEntry();
  MemoryBankEntry(MemoryBankType type,
                  GURL url,
                  std::string tab_title,
                  std::optional<std::string> selected_text = std::nullopt);
  MemoryBankEntry(const MemoryBankEntry&);
  MemoryBankEntry& operator=(const MemoryBankEntry&);
  MemoryBankEntry(MemoryBankEntry&&);
  MemoryBankEntry& operator=(MemoryBankEntry&&);
  ~MemoryBankEntry();

  // The unique identifier of the entry.
  int64_t id = 0;
  // The type of entry.
  MemoryBankType type = MemoryBankType::kTab;
  // The timestamp when the entry was saved to the memory bank.
  base::Time timestamp;
  // The URL of the page where the entry originated.
  GURL url;
  // The title of the tab where the entry originated.
  std::string tab_title;
  // TODO(crbug.com/530253460): Reconsider whether we should save an entry if
  // selected_text is empty.
  // The text that the user selected from the page.
  std::optional<std::string> selected_text;
  // Tags associated with the entry for grouping.
  std::vector<std::string> tags;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_ENTRY_H_

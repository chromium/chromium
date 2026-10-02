// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MOCK_MEMORY_BANK_OBSERVER_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MOCK_MEMORY_BANK_OBSERVER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace context_hub {

class MockMemoryBankObserver : public MemoryBank::Observer {
 public:
  MockMemoryBankObserver();
  ~MockMemoryBankObserver() override;

  MOCK_METHOD(void,
              OnMemoryBankEntryAdded,
              (const MemoryBankEntry&),
              (override));
  MOCK_METHOD(void,
              OnMemoryBankEntryUpdated,
              (int64_t,
               const std::vector<std::string>&,
               const std::optional<std::string>&,
               const std::optional<std::string>&),
              (override));
  MOCK_METHOD(void,
              OnMemoryBankEntriesDeleted,
              (const std::vector<int64_t>&),
              (override));
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MOCK_MEMORY_BANK_OBSERVER_H_

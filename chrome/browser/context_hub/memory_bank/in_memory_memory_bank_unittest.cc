// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

std::vector<MemoryBankEntry> GetAllEntriesSync(const InMemoryMemoryBank& bank) {
  std::vector<MemoryBankEntry> result;
  bank.GetAllEntries(
      base::BindLambdaForTesting([&](std::vector<MemoryBankEntry> entries) {
        result = std::move(entries);
      }));
  return result;
}

}  // namespace

TEST(InMemoryMemoryBankTest, SaveTab) {
  InMemoryMemoryBank memory_bank;
  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());

  GURL url("https://www.google.com");
  memory_bank.SaveTab(url, "Google", "Page text", base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(1u, entries.size());
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(url, entries[0].url);
  EXPECT_EQ("Google", entries[0].tab_title);
  EXPECT_EQ("Page text", entries[0].selected_text);
}

TEST(InMemoryMemoryBankTest, SaveTabWithText) {
  InMemoryMemoryBank memory_bank;
  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());

  GURL url("https://www.google.com");
  memory_bank.SaveTab(url, "Google", "Page Content", base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(1u, entries.size());
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(url, entries[0].url);
  EXPECT_EQ("Google", entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Page Content", entries[0].selected_text.value());
}

TEST(InMemoryMemoryBankTest, SaveTextSelection) {
  InMemoryMemoryBank memory_bank;

  GURL url("https://www.google.com");
  memory_bank.SaveTextSelection(url, "Google", "Search", base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(1u, entries.size());
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(MemoryBankType::kTextSelection, entries[0].type);
  EXPECT_EQ(url, entries[0].url);
  EXPECT_EQ("Google", entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Search", entries[0].selected_text.value());
}

TEST(InMemoryMemoryBankTest, DeleteEntries) {
  InMemoryMemoryBank memory_bank;

  memory_bank.SaveTab(GURL("https://www.google.com"), "Google", "Page text 1",
                      base::DoNothing());
  memory_bank.SaveTab(GURL("https://www.youtube.com"), "YouTube", "Page text 2",
                      base::DoNothing());
  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(2u, entries.size());

  std::vector<int64_t> ids_to_delete = {entries[0].id, entries[1].id};
  memory_bank.DeleteEntries(ids_to_delete, base::DoNothing());

  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());
}

TEST(InMemoryMemoryBankTest, GetEntriesByIds) {
  InMemoryMemoryBank memory_bank;

  memory_bank.SaveTab(GURL("https://www.google.com"), "Google", "Text 1",
                      base::DoNothing());
  memory_bank.SaveTab(GURL("https://www.youtube.com"), "YouTube", "Text 2",
                      base::DoNothing());
  std::vector<MemoryBankEntry> all_entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(2u, all_entries.size());

  std::vector<MemoryBankEntry> retrieved;
  memory_bank.GetEntriesByIds(
      {all_entries[0].id},
      base::BindLambdaForTesting([&](std::vector<MemoryBankEntry> entries) {
        retrieved = std::move(entries);
      }));

  ASSERT_EQ(1u, retrieved.size());
  EXPECT_EQ(all_entries[0].id, retrieved[0].id);
  EXPECT_EQ(all_entries[0].url, retrieved[0].url);
}

}  // namespace context_hub

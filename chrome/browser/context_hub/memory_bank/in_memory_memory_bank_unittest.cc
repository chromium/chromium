// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
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

TEST(InMemoryMemoryBankTest, SaveMemoryBankEntry_Tab) {
  InMemoryMemoryBank memory_bank;
  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());

  GURL url("https://www.google.com");
  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, url, "Google", "Page text"),
      base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(1u, entries.size());
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(url, entries[0].url);
  EXPECT_EQ("Google", entries[0].tab_title);
  EXPECT_EQ("Page text", entries[0].selected_text);
}

TEST(InMemoryMemoryBankTest, SaveMemoryBankEntry_WithText) {
  InMemoryMemoryBank memory_bank;
  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());

  GURL url("https://www.google.com");
  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, url, "Google", "Page Content"),
      base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(1u, entries.size());
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(url, entries[0].url);
  EXPECT_EQ("Google", entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Page Content", entries[0].selected_text.value());
}

TEST(InMemoryMemoryBankTest, SaveMemoryBankEntry_TextSelection) {
  InMemoryMemoryBank memory_bank;

  GURL url("https://www.google.com");
  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTextSelection, url, "Google", "Search"),
      base::DoNothing());

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

  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://www.google.com"),
                      "Google", "Page text 1"),
      base::DoNothing());
  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://www.youtube.com"),
                      "YouTube", "Page text 2"),
      base::DoNothing());

  std::vector<MemoryBankEntry> entries = GetAllEntriesSync(memory_bank);
  ASSERT_EQ(2u, entries.size());

  std::vector<int64_t> ids_to_delete = {entries[0].id, entries[1].id};
  memory_bank.DeleteEntries(ids_to_delete, base::DoNothing());

  EXPECT_TRUE(GetAllEntriesSync(memory_bank).empty());
}

TEST(InMemoryMemoryBankTest, GetEntriesByIds) {
  InMemoryMemoryBank memory_bank;

  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://www.google.com"),
                      "Google", "Text 1"),
      base::DoNothing());
  memory_bank.SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://www.youtube.com"),
                      "YouTube", "Text 2"),
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

TEST(InMemoryMemoryBankTest, GetAllTags) {
  InMemoryMemoryBank memory_bank;

  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example.com/1"),
                         "Tab 1", "Content 1");
  entry1.tags = {"tag1", "tag2"};
  memory_bank.SaveMemoryBankEntry(entry1, base::DoNothing());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example.com/2"),
                         "Tab 2", "Content 2");
  entry2.tags = {"tag2", "tag3"};
  memory_bank.SaveMemoryBankEntry(entry2, base::DoNothing());

  std::vector<std::string> tags;
  memory_bank.GetAllTags(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& result) { tags = result; }));

  EXPECT_THAT(tags, testing::UnorderedElementsAre("tag1", "tag2", "tag3"));
}

TEST(InMemoryMemoryBankTest, GetAllCollections) {
  InMemoryMemoryBank memory_bank;

  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example.com/1"),
                         "Tab 1", "Content 1");
  entry1.collection = "Research";
  memory_bank.SaveMemoryBankEntry(entry1, base::DoNothing());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example.com/2"),
                         "Tab 2", "Content 2");
  entry2.collection = "Recipes";
  memory_bank.SaveMemoryBankEntry(entry2, base::DoNothing());

  std::vector<std::string> collections;
  memory_bank.GetAllCollections(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& result) { collections = result; }));

  EXPECT_THAT(collections, testing::ElementsAre("Recipes", "Research"));
}

}  // namespace context_hub

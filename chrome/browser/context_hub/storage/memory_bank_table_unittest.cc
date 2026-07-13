// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/memory_bank_table.h"

#include <vector>

#include "base/time/time.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

class MemoryBankTableTest : public testing::Test {
 public:
  MemoryBankTableTest() = default;
  ~MemoryBankTableTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.Init(&db_));
    ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());
  }

  sql::Database db_{sql::DatabaseOptions{}, sql::test::kTestTag};
  MemoryBankTable table_;
};

TEST_F(MemoryBankTableTest, AddAndGetEntry) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://www.example.com");
  entry.tab_title = "Example Title";
  entry.selected_text = "Page content";
  entry.tags = {"tag1", "tag2"};

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(1u, entries.size());

  int64_t generated_id = entries[0].id;
  EXPECT_GT(generated_id, 0);
  EXPECT_EQ(MemoryBankType::kTab, entries[0].type);
  EXPECT_EQ(entry.timestamp, entries[0].timestamp);
  EXPECT_EQ(entry.url, entries[0].url);
  EXPECT_EQ(entry.tab_title, entries[0].tab_title);
  ASSERT_TRUE(entries[0].selected_text.has_value());
  EXPECT_EQ("Page content", entries[0].selected_text.value());
  EXPECT_EQ((std::vector<std::string>{"tag1", "tag2"}), entries[0].tags);

  auto fetched_entry = table_.GetEntry(generated_id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ(generated_id, fetched_entry->id);
  EXPECT_EQ(entry.url, fetched_entry->url);
}

TEST_F(MemoryBankTableTest, UpdateExistingEntry) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://www.google.com");
  entry.tab_title = "Google";
  entry.selected_text = "Selected text";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(1u, entries.size());

  MemoryBankEntry updated_entry = entries[0];
  updated_entry.tab_title = "Updated Title";
  updated_entry.selected_text = "New Selection";
  updated_entry.tags = {"updated"};

  EXPECT_TRUE(table_.AddOrUpdateEntry(updated_entry));

  auto fetched_entry = table_.GetEntry(updated_entry.id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ("Updated Title", fetched_entry->tab_title);
  EXPECT_EQ("New Selection", fetched_entry->selected_text.value());
  EXPECT_EQ((std::vector<std::string>{"updated"}), fetched_entry->tags);
}

TEST_F(MemoryBankTableTest, GetAllEntriesOrdering) {
  base::Time base_time = base::Time::FromSecondsSinceUnixEpoch(10000);

  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base_time - base::Hours(2);
  entry1.url = GURL("https://old.com");
  entry1.tab_title = "Old";

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base_time;
  entry2.url = GURL("https://new.com");
  entry2.tab_title = "New";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(2u, entries.size());
  // Ordered by timestamp DESC
  EXPECT_EQ("New", entries[0].tab_title);
  EXPECT_EQ("Old", entries[1].tab_title);
}

TEST_F(MemoryBankTableTest, DeleteEntries) {
  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://site1.com");
  entry1.tab_title = "Site 1";

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://site2.com");
  entry2.tab_title = "Site 2";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(2u, entries.size());

  int64_t id_to_delete = entries[0].id;
  std::vector<int64_t> ids_to_delete = {id_to_delete};
  EXPECT_TRUE(table_.DeleteEntries(ids_to_delete));

  std::vector<MemoryBankEntry> remaining = table_.GetAllEntries();
  ASSERT_EQ(1u, remaining.size());
  EXPECT_NE(id_to_delete, remaining[0].id);
}

TEST_F(MemoryBankTableTest, AddWithExplicitId) {
  MemoryBankEntry entry;
  entry.id = 987654321;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://www.example.com");
  entry.tab_title = "Example Title";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  auto fetched = table_.GetEntry(987654321);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_EQ(987654321, fetched->id);
  EXPECT_EQ(entry.timestamp, fetched->timestamp);
}

TEST_F(MemoryBankTableTest, AddWithoutIdAutoIncrements) {
  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://example1.com");
  entry1.tab_title = "Title 1";

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://example2.com");
  entry2.tab_title = "Title 2";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(2u, entries.size());
  // GetAllEntries orders by timestamp DESC, so entry2 is first.
  EXPECT_EQ(2, entries[0].id);
  EXPECT_EQ(1, entries[1].id);
}

TEST_F(MemoryBankTableTest, NullOptionalFieldsStoredAsSqlNull) {
  MemoryBankEntry entry;
  entry.id = 12345;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://example.com");
  entry.tab_title = "Title";
  entry.selected_text = std::nullopt;
  entry.tags = {};

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT selected_text, tags FROM memory_bank_entries WHERE id = ?"));
  statement.BindInt64(0, 12345);
  ASSERT_TRUE(statement.Step());

  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));

  auto fetched = table_.GetEntry(12345);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_FALSE(fetched->selected_text.has_value());
  EXPECT_TRUE(fetched->tags.empty());
}

}  // namespace context_hub

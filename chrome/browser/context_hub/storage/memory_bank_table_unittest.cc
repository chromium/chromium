// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/memory_bank_table.h"

#include <vector>

#include "base/time/time.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
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
    ASSERT_TRUE(table_.MigrateToVersion2AddNoteAndCollectionColumns());
  }

  sql::Database db_{sql::DatabaseOptions{}, sql::test::kTestTag};
  MemoryBankTable table_;
};

TEST_F(MemoryBankTableTest, MigrateFromCleanStateToVersion1) {
  sql::Database db(sql::DatabaseOptions{}, sql::test::kTestTag);
  ASSERT_TRUE(db.OpenInMemory());
  MemoryBankTable table;
  ASSERT_TRUE(table.Init(&db));
  EXPECT_TRUE(table.MigrateFromCleanStateToVersion1());
  EXPECT_TRUE(db.DoesTableExist("memory_bank_entries"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "id"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "type"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "timestamp"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "url"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "tab_title"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "selected_text"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "tags"));
  EXPECT_FALSE(db.DoesColumnExist("memory_bank_entries", "note"));
  EXPECT_FALSE(db.DoesColumnExist("memory_bank_entries", "collection"));
}

TEST_F(MemoryBankTableTest, MigrateToVersion2AddNoteAndCollectionColumns) {
  sql::Database db(sql::DatabaseOptions{}, sql::test::kTestTag);
  ASSERT_TRUE(db.OpenInMemory());
  MemoryBankTable table;
  ASSERT_TRUE(table.Init(&db));
  ASSERT_TRUE(table.MigrateFromCleanStateToVersion1());

  // Insert a row in version 1 schema.
  ASSERT_TRUE(
      db.Execute("INSERT INTO memory_bank_entries (id, type, timestamp, url, "
                 "tab_title, selected_text, tags) VALUES (1, 0, 1000, "
                 "'https://example.com', 'Title', 'Text', '[\"tag1\"]')"));

  // Run migration to version 2.
  EXPECT_TRUE(table.MigrateToVersion2AddNoteAndCollectionColumns());

  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "note"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "collection"));

  // Verify the existing row can be fetched and note/collection are nullopt.
  auto entry = table.GetEntry(1);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ("Title", entry->tab_title);
  EXPECT_FALSE(entry->note.has_value());
  EXPECT_FALSE(entry->collection.has_value());

  // Verify new row with note and collection can be added and fetched.
  MemoryBankEntry new_entry;
  new_entry.type = MemoryBankType::kTab;
  new_entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  new_entry.url = GURL("https://example2.com");
  new_entry.tab_title = "Title 2";
  new_entry.note = "Note 2";
  new_entry.collection = "Collection 2";
  EXPECT_TRUE(table.AddOrUpdateEntry(new_entry));

  std::vector<MemoryBankEntry> all = table.GetAllEntries();
  EXPECT_EQ(2u, all.size());
}

TEST_F(MemoryBankTableTest, AddAndGetEntry) {
  EXPECT_EQ(0u, table_.GetEntryCount());

  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://www.example.com");
  entry.tab_title = "Example Title";
  entry.selected_text = "Page content";
  entry.tags = {"tag1", "tag2"};
  entry.note = "Test Note";
  entry.collection = "Research";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));
  EXPECT_EQ(1u, table_.GetEntryCount());

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
  ASSERT_TRUE(entries[0].note.has_value());
  EXPECT_EQ("Test Note", entries[0].note.value());
  ASSERT_TRUE(entries[0].collection.has_value());
  EXPECT_EQ("Research", entries[0].collection.value());

  auto fetched_entry = table_.GetEntry(generated_id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ(generated_id, fetched_entry->id);
  EXPECT_EQ(entry.url, fetched_entry->url);
  EXPECT_EQ("Test Note", fetched_entry->note.value());
  EXPECT_EQ("Research", fetched_entry->collection.value());
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
  updated_entry.note = "Updated Note";
  updated_entry.collection = "Updated Collection";

  EXPECT_TRUE(table_.AddOrUpdateEntry(updated_entry));
  EXPECT_EQ(1u, table_.GetEntryCount());

  auto fetched_entry = table_.GetEntry(updated_entry.id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ("Updated Title", fetched_entry->tab_title);
  EXPECT_EQ("New Selection", fetched_entry->selected_text.value());
  EXPECT_EQ((std::vector<std::string>{"updated"}), fetched_entry->tags);
  ASSERT_TRUE(fetched_entry->note.has_value());
  EXPECT_EQ("Updated Note", fetched_entry->note.value());
  ASSERT_TRUE(fetched_entry->collection.has_value());
  EXPECT_EQ("Updated Collection", fetched_entry->collection.value());
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
  EXPECT_EQ(2u, table_.GetEntryCount());

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(2u, entries.size());

  int64_t id_to_delete = entries[0].id;
  std::vector<int64_t> ids_to_delete = {id_to_delete};
  EXPECT_TRUE(table_.DeleteEntries(ids_to_delete));
  EXPECT_EQ(1u, table_.GetEntryCount());

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
  entry.note = std::nullopt;
  entry.collection = std::nullopt;

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT selected_text, tags, note, collection FROM memory_bank_entries "
      "WHERE id = ?"));
  statement.BindInt64(0, 12345);
  ASSERT_TRUE(statement.Step());

  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(3));

  auto fetched = table_.GetEntry(12345);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_FALSE(fetched->selected_text.has_value());
  EXPECT_TRUE(fetched->tags.empty());
  EXPECT_FALSE(fetched->note.has_value());
  EXPECT_FALSE(fetched->collection.has_value());
}

TEST_F(MemoryBankTableTest, GetEntriesByIds) {
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

  std::vector<MemoryBankEntry> all = table_.GetAllEntries();
  ASSERT_EQ(2u, all.size());

  std::vector<MemoryBankEntry> retrieved =
      table_.GetEntriesByIds({all[0].id});
  ASSERT_EQ(1u, retrieved.size());
  EXPECT_EQ(all[0].id, retrieved[0].id);
  EXPECT_EQ(all[0].tab_title, retrieved[0].tab_title);
}

TEST_F(MemoryBankTableTest, GetAllTags) {
  EXPECT_TRUE(table_.GetAllTags().empty());

  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://site1.com");
  entry1.tab_title = "Site 1";
  entry1.tags = {"tagA", "tagB"};

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://site2.com");
  entry2.tab_title = "Site 2";
  entry2.tags = {"tagB", "tagC"};

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));

  std::vector<std::string> tags = table_.GetAllTags();
  EXPECT_THAT(tags, testing::UnorderedElementsAre("tagA", "tagB", "tagC"));
}

TEST_F(MemoryBankTableTest, GetAllCollections) {
  EXPECT_TRUE(table_.GetAllCollections().empty());

  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://site1.com");
  entry1.tab_title = "Site 1";
  entry1.collection = "Research";

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://site2.com");
  entry2.tab_title = "Site 2";
  entry2.collection = "Work";

  MemoryBankEntry entry3;
  entry3.type = MemoryBankType::kTab;
  entry3.timestamp = base::Time::FromSecondsSinceUnixEpoch(3000);
  entry3.url = GURL("https://site3.com");
  entry3.tab_title = "Site 3";
  entry3.collection = "Research";

  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry3));

  std::vector<std::string> collections = table_.GetAllCollections();
  EXPECT_THAT(collections, testing::ElementsAre("Research", "Work"));
}

}  // namespace context_hub

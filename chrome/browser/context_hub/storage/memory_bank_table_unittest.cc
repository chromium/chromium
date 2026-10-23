// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/memory_bank_table.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
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
    ASSERT_TRUE(table_.MigrateToVersion3AddImageUrlColumn());
    ASSERT_TRUE(table_.MigrateToVersion4AddEmbeddingsTables());
  }

  // Decodes the stored embedding vectors for `entry_id`. Returns an empty
  // vector if there is no row or the blob is malformed.
  std::vector<std::vector<float>> GetStoredEmbeddings(int64_t entry_id) {
    sql::Statement statement(db_.GetUniqueStatement(
        "SELECT embedding_blob FROM memory_bank_embeddings "
        "WHERE entry_id = ?"));
    statement.BindInt64(0, entry_id);
    if (!statement.Step()) {
      return {};
    }
    base::span<const uint8_t> blob = statement.ColumnBlob(0);
    if (blob.size() < sizeof(uint32_t)) {
      return {};
    }
    auto [header, payload] = blob.split_at<sizeof(uint32_t)>();
    const size_t dimension = base::U32FromNativeEndian(header);
    if (dimension == 0 || payload.empty() ||
        payload.size() % sizeof(float) != 0) {
      return {};
    }
    const size_t total_floats = payload.size() / sizeof(float);
    if (total_floats % dimension != 0) {
      return {};
    }
    const size_t vector_bytes = dimension * sizeof(float);
    std::vector<std::vector<float>> result;
    while (!payload.empty()) {
      std::vector<float> floats(dimension);
      base::as_writable_byte_span(base::allow_nonunique_obj, floats)
          .copy_from(payload.take_first(vector_bytes));
      result.push_back(std::move(floats));
    }
    return result;
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
  EXPECT_FALSE(db.DoesColumnExist("memory_bank_entries", "image_url"));
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
  EXPECT_FALSE(db.DoesColumnExist("memory_bank_entries", "image_url"));

  // Verify the existing row can be fetched and note/collection are null.
  {
    sql::Statement statement(db.GetUniqueStatement(
        "SELECT tab_title, note, collection FROM memory_bank_entries "
        "WHERE id = 1"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ("Title", statement.ColumnString(0));
    EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
    EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  }

  // Verify new row with note and collection can be added and fetched.
  ASSERT_TRUE(
      db.Execute("INSERT INTO memory_bank_entries (id, type, timestamp, url, "
                 "tab_title, selected_text, tags, note, collection) VALUES "
                 "(2, 0, 2000, 'https://example2.com', 'Title 2', NULL, "
                 "NULL, 'Note 2', 'Collection 2')"));
  {
    sql::Statement statement(db.GetUniqueStatement(
        "SELECT tab_title, note, collection FROM memory_bank_entries "
        "WHERE id = 2"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ("Title 2", statement.ColumnString(0));
    EXPECT_EQ("Note 2", statement.ColumnString(1));
    EXPECT_EQ("Collection 2", statement.ColumnString(2));
  }
}

TEST_F(MemoryBankTableTest, MigrateToVersion3AddImageUrlColumn) {
  sql::Database db(sql::DatabaseOptions{}, sql::test::kTestTag);
  ASSERT_TRUE(db.OpenInMemory());
  MemoryBankTable table;
  ASSERT_TRUE(table.Init(&db));
  ASSERT_TRUE(table.MigrateFromCleanStateToVersion1());
  ASSERT_TRUE(table.MigrateToVersion2AddNoteAndCollectionColumns());

  // Insert a row in version 2 schema.
  ASSERT_TRUE(
      db.Execute("INSERT INTO memory_bank_entries (id, type, timestamp, url, "
                 "tab_title, selected_text, tags, note, collection) VALUES "
                 "(1, 0, 1000, 'https://example.com', 'Title', 'Text', "
                 "'[\"tag1\"]', 'Note 1', 'Collection 1')"));

  // Run migration to version 3.
  EXPECT_TRUE(table.MigrateToVersion3AddImageUrlColumn());
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_entries", "image_url"));

  // Verify the existing row can be fetched and image_url is nullopt.
  auto entry = table.GetEntry(1);
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ("Title", entry->tab_title);
  EXPECT_EQ("Note 1", entry->note);
  EXPECT_EQ("Collection 1", entry->collection);
  EXPECT_FALSE(entry->image_url.has_value());

  // Verify new row with image_url can be added and fetched.
  MemoryBankEntry new_entry;
  new_entry.type = MemoryBankType::kImage;
  new_entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  new_entry.url = GURL("https://example2.com");
  new_entry.tab_title = "Title 2";
  new_entry.image_url = GURL("https://example2.com/image.png");
  new_entry.note = "Note 2";
  new_entry.collection = "Collection 2";
  EXPECT_THAT(table.AddOrUpdateEntry(new_entry), testing::Optional(2));

  std::vector<MemoryBankEntry> all = table.GetAllEntries();
  ASSERT_EQ(2u, all.size());
  EXPECT_EQ(MemoryBankType::kImage, all[0].type);
  EXPECT_EQ(GURL("https://example2.com/image.png"), all[0].image_url);
}

TEST_F(MemoryBankTableTest, MigrateToVersion4AddEmbeddingsTables) {
  sql::Database db(sql::DatabaseOptions{}, sql::test::kTestTag);
  ASSERT_TRUE(db.OpenInMemory());
  MemoryBankTable table;
  ASSERT_TRUE(table.Init(&db));
  ASSERT_TRUE(table.MigrateFromCleanStateToVersion1());
  ASSERT_TRUE(table.MigrateToVersion2AddNoteAndCollectionColumns());
  ASSERT_TRUE(table.MigrateToVersion3AddImageUrlColumn());

  EXPECT_FALSE(db.DoesTableExist("memory_bank_embeddings"));
  EXPECT_FALSE(db.DoesTableExist("memory_bank_embeddings_meta"));

  EXPECT_TRUE(table.MigrateToVersion4AddEmbeddingsTables());

  EXPECT_TRUE(db.DoesTableExist("memory_bank_embeddings"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_embeddings", "entry_id"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_embeddings", "embedding_blob"));
  EXPECT_TRUE(db.DoesTableExist("memory_bank_embeddings_meta"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_embeddings_meta", "name"));
  EXPECT_TRUE(db.DoesColumnExist("memory_bank_embeddings_meta", "value"));
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry), testing::Optional(1));
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
  EXPECT_FALSE(entries[0].image_url.has_value());
  EXPECT_EQ((std::vector<std::string>{"tag1", "tag2"}), entries[0].tags);
  ASSERT_TRUE(entries[0].note.has_value());
  EXPECT_EQ("Test Note", entries[0].note.value());
  ASSERT_TRUE(entries[0].collection.has_value());
  EXPECT_EQ("Research", entries[0].collection.value());

  auto fetched_entry = table_.GetEntry(generated_id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ(generated_id, fetched_entry->id);
  EXPECT_EQ(entry.url, fetched_entry->url);
  EXPECT_FALSE(fetched_entry->image_url.has_value());
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry), testing::Optional(1));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(1u, entries.size());

  MemoryBankEntry updated_entry = entries[0];
  updated_entry.tab_title = "Updated Title";
  updated_entry.selected_text = "New Selection";
  updated_entry.image_url = GURL("https://www.google.com/logo.png");
  updated_entry.tags = {"updated"};
  updated_entry.note = "Updated Note";
  updated_entry.collection = "Updated Collection";

  EXPECT_THAT(table_.AddOrUpdateEntry(updated_entry),
              testing::Optional(updated_entry.id));
  EXPECT_EQ(1u, table_.GetEntryCount());

  auto fetched_entry = table_.GetEntry(updated_entry.id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ("Updated Title", fetched_entry->tab_title);
  EXPECT_EQ("New Selection", fetched_entry->selected_text.value());
  ASSERT_TRUE(fetched_entry->image_url.has_value());
  EXPECT_EQ(GURL("https://www.google.com/logo.png"),
            fetched_entry->image_url.value());
  EXPECT_EQ((std::vector<std::string>{"updated"}), fetched_entry->tags);
  ASSERT_TRUE(fetched_entry->note.has_value());
  EXPECT_EQ("Updated Note", fetched_entry->note.value());
  ASSERT_TRUE(fetched_entry->collection.has_value());
  EXPECT_EQ("Updated Collection", fetched_entry->collection.value());
}

TEST_F(MemoryBankTableTest, UpdateEntryAnnotations) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://www.google.com");
  entry.tab_title = "Original Title";
  entry.selected_text = "Original Selected Text";
  entry.tags = {"tag1"};
  entry.note = "Original Note";
  entry.collection = "General";

  EXPECT_THAT(table_.AddOrUpdateEntry(entry), testing::Optional(1));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(1u, entries.size());
  int64_t id = entries[0].id;

  EXPECT_TRUE(table_.UpdateEntryAnnotations(id, {"new_tag1", "new_tag2"},
                                            "New Note", "Work"));

  auto fetched_entry = table_.GetEntry(id);
  ASSERT_TRUE(fetched_entry.has_value());
  EXPECT_EQ("Original Title", fetched_entry->tab_title);
  EXPECT_EQ("Original Selected Text", fetched_entry->selected_text.value());
  EXPECT_THAT(fetched_entry->tags,
              testing::ElementsAre("new_tag1", "new_tag2"));
  ASSERT_TRUE(fetched_entry->note.has_value());
  EXPECT_EQ("New Note", fetched_entry->note.value());
  ASSERT_TRUE(fetched_entry->collection.has_value());
  EXPECT_EQ("Work", fetched_entry->collection.value());
}

TEST_F(MemoryBankTableTest, UpdateEntryAnnotations_NotFound) {
  EXPECT_FALSE(
      table_.UpdateEntryAnnotations(999999, {"tag1"}, "Note", "Collection"));
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));

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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry), testing::Optional(987654321));

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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));

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
  entry.image_url = std::nullopt;
  entry.tags = {};
  entry.note = std::nullopt;
  entry.collection = std::nullopt;

  EXPECT_THAT(table_.AddOrUpdateEntry(entry), testing::Optional(12345));

  sql::Statement statement(db_.GetUniqueStatement(
      "SELECT selected_text, tags, note, collection, image_url FROM "
      "memory_bank_entries WHERE id = ?"));
  statement.BindInt64(0, 12345);
  ASSERT_TRUE(statement.Step());

  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(0));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(1));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(2));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(3));
  EXPECT_EQ(sql::ColumnType::kNull, statement.GetColumnType(4));

  auto fetched = table_.GetEntry(12345);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_FALSE(fetched->selected_text.has_value());
  EXPECT_TRUE(fetched->tags.empty());
  EXPECT_FALSE(fetched->note.has_value());
  EXPECT_FALSE(fetched->collection.has_value());
  EXPECT_FALSE(fetched->image_url.has_value());
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));

  std::vector<MemoryBankEntry> all = table_.GetAllEntries();
  ASSERT_EQ(2u, all.size());

  std::vector<MemoryBankEntry> retrieved = table_.GetEntriesByIds({all[0].id});
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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));

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

  EXPECT_THAT(table_.AddOrUpdateEntry(entry1), testing::Optional(1));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry2), testing::Optional(2));
  EXPECT_THAT(table_.AddOrUpdateEntry(entry3), testing::Optional(3));

  std::vector<std::string> collections = table_.GetAllCollections();
  EXPECT_THAT(collections, testing::ElementsAre("Research", "Work"));
}

TEST_F(MemoryBankTableTest, AddOrUpdateEmbeddings) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://example.com");
  entry.tab_title = "Example";
  entry.selected_text = "Page text";
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(1u, entries.size());
  int64_t entry_id = entries[0].id;

  // No embeddings initially.
  EXPECT_THAT(table_.GetEntryIdsWithoutEmbeddings(),
              testing::ElementsAre(entry_id));

  // Empty embeddings and invalid or non-existent IDs are rejected.
  EXPECT_FALSE(table_.AddOrUpdateEmbeddings(entry_id, {}));
  EXPECT_FALSE(table_.AddOrUpdateEmbeddings(
      0, {passage_embeddings::Embedding({1.0f, 0.0f})}));
  EXPECT_FALSE(table_.AddOrUpdateEmbeddings(
      999, {passage_embeddings::Embedding({1.0f, 0.0f})}));
  EXPECT_THAT(table_.GetEntryIdsWithoutEmbeddings(),
              testing::ElementsAre(entry_id));

  // Add multiple passage embeddings for the entry.
  EXPECT_TRUE(table_.AddOrUpdateEmbeddings(
      entry_id, {passage_embeddings::Embedding({0.6f, 0.8f}),
                 passage_embeddings::Embedding({0.8f, 0.6f})}));
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());
  EXPECT_THAT(GetStoredEmbeddings(entry_id),
              testing::ElementsAre(testing::ElementsAre(0.6f, 0.8f),
                                   testing::ElementsAre(0.8f, 0.6f)));

  // Update embeddings with fewer passages; the old vectors are replaced.
  EXPECT_TRUE(table_.AddOrUpdateEmbeddings(
      entry_id, {passage_embeddings::Embedding({1.0f, 0.0f})}));
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());
  EXPECT_THAT(GetStoredEmbeddings(entry_id),
              testing::ElementsAre(testing::ElementsAre(1.0f, 0.0f)));

  // Embeddings with mismatched dimensions are rejected and leave the stored
  // embeddings intact.
  EXPECT_FALSE(table_.AddOrUpdateEmbeddings(
      entry_id, {passage_embeddings::Embedding({0.0f, 1.0f}),
                 passage_embeddings::Embedding({0.0f, 0.0f, 1.0f})}));
  EXPECT_THAT(GetStoredEmbeddings(entry_id),
              testing::ElementsAre(testing::ElementsAre(1.0f, 0.0f)));
}

TEST_F(MemoryBankTableTest, DeleteEntriesDeletesEmbeddings) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTextSelection;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://example.com");
  entry.tab_title = "Example";
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry));

  int64_t entry_id = table_.GetAllEntries()[0].id;
  EXPECT_TRUE(table_.AddOrUpdateEmbeddings(
      entry_id, {passage_embeddings::Embedding({1.0f, 0.0f})}));
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());

  EXPECT_TRUE(table_.DeleteEntries({entry_id}));
  EXPECT_FALSE(table_.GetEntry(entry_id).has_value());

  // The embeddings row should be gone along with its entry.
  sql::Statement count_statement(
      db_.GetUniqueStatement("SELECT COUNT(*) FROM memory_bank_embeddings"));
  ASSERT_TRUE(count_statement.Step());
  EXPECT_EQ(0, count_statement.ColumnInt(0));
}

TEST_F(MemoryBankTableTest, GetEntryIdsWithoutEmbeddings) {
  MemoryBankEntry entry1;
  entry1.type = MemoryBankType::kTab;
  entry1.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry1.url = GURL("https://example1.com");
  entry1.tab_title = "Title 1";
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry1));

  MemoryBankEntry entry2;
  entry2.type = MemoryBankType::kTab;
  entry2.timestamp = base::Time::FromSecondsSinceUnixEpoch(2000);
  entry2.url = GURL("https://example2.com");
  entry2.tab_title = "Title 2";
  EXPECT_TRUE(table_.AddOrUpdateEntry(entry2));

  std::vector<MemoryBankEntry> entries = table_.GetAllEntries();
  ASSERT_EQ(2u, entries.size());
  int64_t id1 = entries[0].id;
  int64_t id2 = entries[1].id;

  EXPECT_THAT(table_.GetEntryIdsWithoutEmbeddings(),
              testing::UnorderedElementsAre(id1, id2));

  EXPECT_TRUE(table_.AddOrUpdateEmbeddings(
      id1, {passage_embeddings::Embedding({1.0f, 0.0f})}));
  EXPECT_THAT(table_.GetEntryIdsWithoutEmbeddings(), testing::ElementsAre(id2));

  EXPECT_TRUE(table_.AddOrUpdateEmbeddings(
      id2, {passage_embeddings::Embedding({0.0f, 1.0f})}));
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());
}

TEST_F(MemoryBankTableTest, SetEmbeddingsVersion) {
  MemoryBankEntry entry;
  entry.type = MemoryBankType::kTab;
  entry.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
  entry.url = GURL("https://example.com");
  entry.tab_title = "Title";
  ASSERT_TRUE(table_.AddOrUpdateEntry(entry));
  int64_t entry_id = table_.GetAllEntries()[0].id;

  // No version initially, and non-positive versions are rejected.
  EXPECT_FALSE(table_.GetEmbeddingsVersion().has_value());
  EXPECT_FALSE(table_.SetEmbeddingsVersion(0));
  EXPECT_FALSE(table_.SetEmbeddingsVersion(-1));

  // Setting the first version on an empty embeddings table succeeds.
  EXPECT_TRUE(table_.SetEmbeddingsVersion(1));
  EXPECT_EQ(1, table_.GetEmbeddingsVersion());

  ASSERT_TRUE(table_.AddOrUpdateEmbeddings(
      entry_id, {passage_embeddings::Embedding({1.0f, 0.0f})}));

  // Non-positive versions do not wipe existing embeddings.
  EXPECT_FALSE(table_.SetEmbeddingsVersion(0));
  EXPECT_EQ(1, table_.GetEmbeddingsVersion());
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());

  // Setting the same version keeps existing embeddings.
  EXPECT_TRUE(table_.SetEmbeddingsVersion(1));
  EXPECT_EQ(1, table_.GetEmbeddingsVersion());
  EXPECT_TRUE(table_.GetEntryIdsWithoutEmbeddings().empty());

  // Changing the version drops all embeddings but keeps entries.
  EXPECT_TRUE(table_.SetEmbeddingsVersion(2));
  EXPECT_EQ(2, table_.GetEmbeddingsVersion());
  EXPECT_TRUE(table_.GetEntry(entry_id).has_value());
  EXPECT_THAT(table_.GetEntryIdsWithoutEmbeddings(),
              testing::ElementsAre(entry_id));
}

}  // namespace context_hub

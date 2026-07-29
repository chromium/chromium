// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/storage/context_hub_database.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/features.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {

class ContextHubDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<ContextHubDatabase>();
  }

  MemoryBankEntry CreateTestData() {
    MemoryBankEntry data;
    data.type = MemoryBankType::kTab;
    data.timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
    data.url = GURL("https://example.com");
    data.tab_title = "Example Title";
    data.selected_text = "Page content";
    data.tags = {"tag1", "tag2"};
    return data;
  }

 protected:
  base::FilePath GetDbPath() const {
    return temp_dir_.GetPath().AppendASCII("TestDB");
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ContextHubDatabase> db_;
};

// Tests that all migrations/initialization from an empty database succeed.
TEST_F(ContextHubDatabaseTest, InitializeEmptyToCurrent) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(db_->Init(GetDbPath()));
  db_.reset();

  // Verify post-conditions. These are expectations for current version of the
  // database.
  {
    sql::Database connection(sql::test::kTestTag);
    ASSERT_TRUE(connection.Open(GetDbPath()));

    sql::Statement get_user_version_stm(
        connection.GetUniqueStatement("PRAGMA user_version"));
    ASSERT_TRUE(get_user_version_stm.is_valid());
    ASSERT_TRUE(get_user_version_stm.Step());
    int detected_user_version = get_user_version_stm.ColumnInt(0);
    EXPECT_EQ(ContextHubDatabase::kCurrentVersionNumber, detected_user_version);

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("memory_bank_entries"));
  }
}

// Tests that all initialization from an existing database succeed.
TEST_F(ContextHubDatabaseTest, InitializeWithExistingDatabase) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(db_->Init(GetDbPath()));
  db_.reset();

  // Re-initialize the database.
  ContextHubDatabase db;
  EXPECT_TRUE(db.Init(GetDbPath()));
}

// Tests that not a SQLite file is handled by deleting and recreating the
// database.
TEST_F(ContextHubDatabaseTest, InitializeWithCorruptFile) {
  // Create a non-SQLite file at the database path.
  ASSERT_TRUE(base::WriteFile(GetDbPath(), "This is not a SQLite file"));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(static_cast<int>(sql::SqliteResultCode::kNotADatabase));

  // Initialize the database. This should detect the corrupt file, delete it,
  // and create a new one.
  EXPECT_TRUE(db_->Init(GetDbPath()));

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

// Tests that all migrations/initialization from a newer database no-ops.
TEST_F(ContextHubDatabaseTest, InitializeGreaterVersionThanCurrent) {
  // Set the user-version to a version greater than the current version.
  sql::Database connection(sql::test::kTestTag);
  ASSERT_TRUE(connection.Open(GetDbPath()));
  ASSERT_TRUE(connection.Execute("PRAGMA user_version=1000000"));
  connection.Close();

  EXPECT_FALSE(db_->Init(GetDbPath()));
}

// Tests that calling methods before initialization fails and returns the
// expected default values.
TEST_F(ContextHubDatabaseTest, CallMemoryBankMethodsBeforeInit) {
  EXPECT_FALSE(db_->AddOrUpdateMemoryBankEntry(CreateTestData()));
  EXPECT_FALSE(db_->GetMemoryBankEntry(1).has_value());
  EXPECT_TRUE(db_->GetMemoryBankEntriesByIds({1}).empty());
  EXPECT_TRUE(db_->GetAllMemoryBankEntries().empty());
  EXPECT_FALSE(db_->DeleteMemoryBankEntries({1}));
}

// Tests retrieving memory bank entries by IDs.
TEST_F(ContextHubDatabaseTest, GetMemoryBankEntriesByIds) {
  ASSERT_TRUE(db_->Init(GetDbPath()));

  MemoryBankEntry entry1 = CreateTestData();
  entry1.url = GURL("https://example1.com");
  entry1.tab_title = "Site 1";
  MemoryBankEntry entry2 = CreateTestData();
  entry2.url = GURL("https://example2.com");
  entry2.tab_title = "Site 2";

  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry1));
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry2));

  std::vector<MemoryBankEntry> all_entries = db_->GetAllMemoryBankEntries();
  ASSERT_EQ(all_entries.size(), 2u);

  std::vector<MemoryBankEntry> retrieved =
      db_->GetMemoryBankEntriesByIds({all_entries[0].id});
  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].id, all_entries[0].id);
  EXPECT_EQ(retrieved[0].tab_title, all_entries[0].tab_title);
}

// Tests adding and retrieving memory bank entries.
TEST_F(ContextHubDatabaseTest, AddAndGetMemoryBankEntry) {
  ASSERT_TRUE(db_->Init(GetDbPath()));

  MemoryBankEntry data = CreateTestData();

  // Successfully add the memory bank entry to the database.
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(data));

  std::vector<MemoryBankEntry> all_entries = db_->GetAllMemoryBankEntries();
  ASSERT_EQ(all_entries.size(), 1u);

  // Retrieve the memory bank entry from the database and verify its contents.
  std::optional<MemoryBankEntry> retrieved =
      db_->GetMemoryBankEntry(all_entries[0].id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->type, data.type);
  EXPECT_EQ(retrieved->timestamp, data.timestamp);
  EXPECT_EQ(retrieved->url, data.url);
  EXPECT_EQ(retrieved->tab_title, data.tab_title);
  EXPECT_EQ(retrieved->selected_text, data.selected_text);
  EXPECT_EQ(retrieved->tags, data.tags);
}

// Tests retrieving a non-existent memory bank entry.
TEST_F(ContextHubDatabaseTest, GetNonExistentMemoryBankEntry) {
  ASSERT_TRUE(db_->Init(GetDbPath()));

  EXPECT_FALSE(db_->GetMemoryBankEntry(999).has_value());
}

// Tests retrieving all memory bank entries.
TEST_F(ContextHubDatabaseTest, GetAllMemoryBankEntries) {
  ASSERT_TRUE(db_->Init(GetDbPath()));

  MemoryBankEntry entry1 = CreateTestData();
  entry1.url = GURL("https://example1.com");
  MemoryBankEntry entry2 = CreateTestData();
  entry2.url = GURL("https://example2.com");

  // Add two memory bank entries to the database successfully.
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry1));
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry2));

  // Verify that both memory bank entries are retrieved from the database.
  std::vector<MemoryBankEntry> all_entries = db_->GetAllMemoryBankEntries();
  ASSERT_EQ(all_entries.size(), 2u);
}

// Tests deleting memory bank entries.
TEST_F(ContextHubDatabaseTest, DeleteMemoryBankEntries) {
  ASSERT_TRUE(db_->Init(GetDbPath()));

  MemoryBankEntry entry1 = CreateTestData();
  entry1.url = GURL("https://site1.com");
  MemoryBankEntry entry2 = CreateTestData();
  entry2.url = GURL("https://site2.com");
  MemoryBankEntry entry3 = CreateTestData();
  entry3.url = GURL("https://site3.com");

  // Add the memory bank entries to the database successfully.
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry1));
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry2));
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry3));

  // Verify that the memory bank entries are present in the database.
  std::vector<MemoryBankEntry> all_entries = db_->GetAllMemoryBankEntries();
  ASSERT_EQ(all_entries.size(), 3u);

  int64_t id_1 = all_entries[0].id;
  int64_t id_2 = all_entries[1].id;
  int64_t id_3 = all_entries[2].id;

  // Delete multiple memory bank entries, including one that doesn't exist.
  EXPECT_TRUE(db_->DeleteMemoryBankEntries({id_1, id_2, 999}));

  // Verify that the memory bank entries are deleted from the database and the
  // expected memory bank entry remains.
  EXPECT_EQ(db_->GetAllMemoryBankEntries().size(), 1u);
  EXPECT_FALSE(db_->GetMemoryBankEntry(id_1).has_value());
  EXPECT_FALSE(db_->GetMemoryBankEntry(id_2).has_value());
  EXPECT_TRUE(db_->GetMemoryBankEntry(id_3).has_value());
}

// Tests that reaching max_memory_bank_entries rejects adding new entries.
TEST_F(ContextHubDatabaseTest, MaxEntriesLimitRejectsNewEntries) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kMemoryBanks, {{"max_memory_bank_entries", "2"}});

  ASSERT_TRUE(db_->Init(GetDbPath()));

  MemoryBankEntry entry1 = CreateTestData();
  entry1.url = GURL("https://example.com/1");
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry1));

  MemoryBankEntry entry2 = CreateTestData();
  entry2.url = GURL("https://example.com/2");
  EXPECT_TRUE(db_->AddOrUpdateMemoryBankEntry(entry2));

  // Limit of 2 entries reached. Attempting to add a 3rd new entry should fail.
  MemoryBankEntry entry3 = CreateTestData();
  entry3.url = GURL("https://example.com/3");
  EXPECT_FALSE(db_->AddOrUpdateMemoryBankEntry(entry3));

  // Table should still contain only 2 entries.
  EXPECT_EQ(db_->GetAllMemoryBankEntries().size(), 2u);
}

}  // namespace context_hub

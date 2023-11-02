// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/database_string_table.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class DatabaseStringTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file = temp_dir_.GetPath().AppendASCII("StringTable.db");

    ASSERT_TRUE(db_.Open(db_file));
  }

  void TearDown() override { db_.Close(); }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};

// Check that initializing the database works.
TEST_F(DatabaseStringTableTest, Init) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);
  ASSERT_TRUE(db_.DoesTableExist("test"));
  ASSERT_TRUE(db_.DoesIndexExist("test_index"));
}

// Insert a new mapping into the table, then verify the table contents.
TEST_F(DatabaseStringTableTest, Insert) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);
  int64_t id;
  ASSERT_TRUE(table.StringToInt(&db_, "abc", &id));

  sql::Statement query(
      db_.GetUniqueStatement("SELECT id FROM test WHERE value = 'abc'"));
  ASSERT_TRUE(query.Step());
  int64_t raw_id = query.ColumnInt64(0);
  ASSERT_EQ(id, raw_id);
}

// Check that different strings are mapped to different values, and the same
// string is mapped to the same value repeatably.
TEST_F(DatabaseStringTableTest, InsertMultiple) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);

  int64_t id1;
  int64_t id2;
  ASSERT_TRUE(table.StringToInt(&db_, "string1", &id1));
  ASSERT_TRUE(table.StringToInt(&db_, "string2", &id2));
  ASSERT_NE(id1, id2);

  int64_t id1a;
  ASSERT_TRUE(table.StringToInt(&db_, "string1", &id1a));
  ASSERT_EQ(id1, id1a);
}

// Check that values can be read back from the database even after the
// in-memory cache is cleared.
TEST_F(DatabaseStringTableTest, CacheCleared) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);

  int64_t id1;
  ASSERT_TRUE(table.StringToInt(&db_, "string1", &id1));

  table.ClearCache();

  int64_t id2;
  ASSERT_TRUE(table.StringToInt(&db_, "string1", &id2));
  ASSERT_EQ(id1, id2);
}

// Check that direct database modifications are picked up after the cache is
// cleared.
TEST_F(DatabaseStringTableTest, DatabaseModified) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);

  int64_t id1;
  ASSERT_TRUE(table.StringToInt(&db_, "modified", &id1));

  ASSERT_TRUE(
      db_.Execute("UPDATE test SET id = id + 1 WHERE value = 'modified'"));

  int64_t id2;
  ASSERT_TRUE(table.StringToInt(&db_, "modified", &id2));
  ASSERT_EQ(id1, id2);

  table.ClearCache();

  int64_t id3;
  ASSERT_TRUE(table.StringToInt(&db_, "modified", &id3));
  ASSERT_EQ(id1 + 1, id3);
}

// Check that looking up an unknown id returns an error.
TEST_F(DatabaseStringTableTest, BadLookup) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);
  std::string value;
  ASSERT_FALSE(table.IntToString(&db_, 1, &value));
}

// Check looking up an inserted value, both cached and not cached.
TEST_F(DatabaseStringTableTest, Lookup) {
  DatabaseStringTable table("test");
  table.Initialize(&db_);
  int64_t id;
  ASSERT_TRUE(table.StringToInt(&db_, "abc", &id));

  std::string value;
  ASSERT_TRUE(table.IntToString(&db_, id, &value));
  ASSERT_EQ("abc", value);

  table.ClearCache();
  value = "";
  ASSERT_TRUE(table.IntToString(&db_, id, &value));
  ASSERT_EQ("abc", value);
}

// Check that the in-memory cache for the string table does not become too
// large, even if many items are inserted.
TEST_F(DatabaseStringTableTest, Prune) {
  DatabaseStringTable table("size_test");
  table.Initialize(&db_);

  // Wrap the lookups in a transaction to improve performance.
  sql::Transaction transaction(&db_);

  ASSERT_TRUE(transaction.Begin());
  for (int i = 0; i < 2000; i++) {
    int64_t id;
    ASSERT_TRUE(table.StringToInt(&db_, base::StringPrintf("value-%d", i),
                                  &id));
  }
  transaction.Commit();

  // The maximum size below should correspond to kMaximumCacheSize in
  // database_string_table.cc, with a small amount of additional slop (an entry
  // might be inserted after doing the pruning).
  ASSERT_LE(table.id_to_value_.size(), 1005U);
  ASSERT_LE(table.value_to_id_.size(), 1005U);
}

}  // namespace extensions

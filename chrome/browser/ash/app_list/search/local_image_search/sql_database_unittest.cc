// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"

#include <tuple>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

constexpr char INSERT_QUERY[] =
    // clang-format off
          "INSERT OR REPLACE INTO test(key,value) "
          "VALUES(?,?)";
// clang-format on

constexpr char SELECT_ALL_QUERY[] =
    // clang-format off
          "SELECT key,value "
          "FROM test";
// clang-format on

int CreateTestSchema(SqlDatabase* db) {
  static constexpr char query[] =
      // clang-format off
            "CREATE TABLE test("
              "key TEXT NOT NULL,"
              "value TEXT NOT NULL)";
  // clang-format on
  db->GetStatementForQuery(SQL_FROM_HERE, query)->Run();
  // Returns current version number
  return 3;
}

int CreateOldTestSchema(SqlDatabase* db) {
  static constexpr char query[] =
      // clang-format off
            "CREATE TABLE test("
              "key TEXT NOT NULL)";
  // clang-format on
  db->GetStatementForQuery(SQL_FROM_HERE, query)->Run();
  return 2;
}

int MigrateTestSchema(SqlDatabase* db, int current_version_number) {
  DCHECK_EQ(current_version_number, 2);
  static constexpr char query[] =
      // clang-format off
            "ALTER TABLE test "
              "ADD value TEXT";
  // clang-format on
  db->GetStatementForQuery(SQL_FROM_HERE, query)->Run();
  return 3;
}

int MigrateOldTestSchema(SqlDatabase* db, int current_version_number) {
  DCHECK_EQ(current_version_number, 2);
  return current_version_number;
}

class SqlDatabaseTest : public testing::Test {
 protected:
  // testing::Test overrides:
  void SetUp() override {
    base::ScopedTempDir temp_dir;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());

    test_directory_ = temp_dir.GetPath();
    const base::FilePath test_db = test_directory_.AppendASCII("test.db");
    sql_database_ = std::make_unique<SqlDatabase>(
        std::move(test_db), /*histogram_tag=*/"test",
        /*current_version_number=*/3, base::BindRepeating(CreateTestSchema),
        base::BindRepeating(MigrateTestSchema));
  }

  void TearDown() override { sql_database_->Close(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SqlDatabase> sql_database_;
  base::FilePath test_directory_;
  int steps_ = 0;
};

TEST_F(SqlDatabaseTest, EmptyStorage) {
  EXPECT_TRUE(sql_database_->Initialize());

  auto statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, SELECT_ALL_QUERY);
  ASSERT_TRUE(statement);

  while (statement->Step()) {
    steps_ += 1;
  }
  EXPECT_EQ(steps_, 0);
  EXPECT_TRUE(statement->Succeeded());
}

TEST_F(SqlDatabaseTest, Insert) {
  EXPECT_TRUE(sql_database_->Initialize());

  auto insert_statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, INSERT_QUERY);
  ASSERT_TRUE(insert_statement);

  insert_statement->BindString(0, "test");
  insert_statement->BindString(1, "123");
  EXPECT_TRUE(insert_statement->Run());

  auto select_statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, SELECT_ALL_QUERY);
  ASSERT_TRUE(select_statement);

  while (select_statement->Step()) {
    EXPECT_EQ(select_statement->ColumnString(0), "test");
    EXPECT_EQ(select_statement->ColumnString(1), "123");
    steps_ += 1;
  }
  // To make sure, it goes inside the loop.
  EXPECT_EQ(steps_, 1);
  EXPECT_TRUE(select_statement->Succeeded());
}

TEST_F(SqlDatabaseTest, Persistence) {
  EXPECT_TRUE(sql_database_->Initialize());

  auto insert_statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, INSERT_QUERY);
  ASSERT_TRUE(insert_statement);

  insert_statement->BindString(0, "test");
  insert_statement->BindString(1, "123");
  EXPECT_TRUE(insert_statement->Run());

  insert_statement->Clear();
  sql_database_->Close();
  EXPECT_TRUE(sql_database_->Initialize());

  auto select_statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, SELECT_ALL_QUERY);
  ASSERT_TRUE(select_statement);

  while (select_statement->Step()) {
    EXPECT_EQ(select_statement->ColumnString(0), "test");
    EXPECT_EQ(select_statement->ColumnString(1), "123");
    steps_ += 1;
  }
  EXPECT_EQ(steps_, 1);
  EXPECT_TRUE(select_statement->Succeeded());
}

TEST_F(SqlDatabaseTest, Downgrade) {
  EXPECT_TRUE(sql_database_->Initialize());

  auto insert_statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, INSERT_QUERY);
  ASSERT_TRUE(insert_statement);

  insert_statement->BindString(0, "test");
  insert_statement->BindString(1, "123");
  EXPECT_TRUE(insert_statement->Run());

  insert_statement->Clear();
  sql_database_->Close();

  sql_database_ = std::make_unique<SqlDatabase>(
      test_directory_.AppendASCII("test.db"), /*histogram_tag=*/"test",
      /*current_version_number=*/2, base::BindRepeating(CreateOldTestSchema),
      base::BindRepeating(MigrateOldTestSchema));

  // Should raze the current db and make an older version.
  EXPECT_TRUE(sql_database_->Initialize());

  auto select_statement = sql_database_->GetStatementForQuery(
      SQL_FROM_HERE, "SELECT key FROM test");
  ASSERT_TRUE(select_statement);

  while (select_statement->Step()) {
    EXPECT_EQ(select_statement->ColumnString(0), "test");
    steps_ += 0;
  }
  EXPECT_EQ(steps_, 0);
  EXPECT_TRUE(select_statement->Succeeded());

  auto insert_statement1 = sql_database_->GetStatementForQuery(
      SQL_FROM_HERE, "INSERT INTO test(key) VALUES(?)");
  ASSERT_TRUE(insert_statement1);

  insert_statement1->BindString(0, "test");
  EXPECT_TRUE(insert_statement1->Run());

  auto select_statement1 = sql_database_->GetStatementForQuery(
      SQL_FROM_HERE, "SELECT key FROM test");
  ASSERT_TRUE(select_statement1);

  while (select_statement1->Step()) {
    EXPECT_EQ(select_statement1->ColumnString(0), "test");
    steps_ += 1;
  }
  EXPECT_EQ(steps_, 1);
  EXPECT_TRUE(select_statement1->Succeeded());
}

TEST_F(SqlDatabaseTest, Upgrade) {
  sql_database_ = std::make_unique<SqlDatabase>(
      test_directory_.AppendASCII("test.db"), /*histogram_tag=*/"test",
      /*current_version_number=*/2, base::BindRepeating(CreateOldTestSchema),
      base::BindRepeating(MigrateOldTestSchema));
  EXPECT_TRUE(sql_database_->Initialize());

  auto insert_statement = sql_database_->GetStatementForQuery(
      SQL_FROM_HERE, "INSERT INTO test(key) VALUES(?)");
  ASSERT_TRUE(insert_statement);

  insert_statement->BindString(0, "test");
  EXPECT_TRUE(insert_statement->Run());

  insert_statement->Clear();
  sql_database_->Close();

  sql_database_ = std::make_unique<SqlDatabase>(
      test_directory_.AppendASCII("test.db"), /*histogram_tag=*/"test",
      /*current_version_number=*/3, base::BindRepeating(CreateTestSchema),
      base::BindRepeating(MigrateTestSchema));
  EXPECT_TRUE(sql_database_->Initialize());

  auto insert_statement1 =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, INSERT_QUERY);
  ASSERT_TRUE(insert_statement1);

  insert_statement1->BindString(0, "foo");
  insert_statement1->BindString(1, "456");
  EXPECT_TRUE(insert_statement1->Run());

  auto select_statement1 =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, SELECT_ALL_QUERY);
  ASSERT_TRUE(select_statement1);

  while (select_statement1->Step()) {
    auto row = std::make_tuple(select_statement1->ColumnString(0),
                               select_statement1->ColumnString(1));
    EXPECT_THAT(row, testing::AnyOfArray({std::make_tuple("test", ""),
                                          std::make_tuple("foo", "456")}));
    steps_ += 1;
  }
  EXPECT_EQ(steps_, 2);
  EXPECT_TRUE(select_statement1->Succeeded());
}

TEST_F(SqlDatabaseTest, InitializationFail) {
  sql_database_ = std::make_unique<SqlDatabase>(
      base::FilePath("/wrong_dir.db"), /*histogram_tag=*/"test",
      /*current_version_number=*/3, base::BindRepeating(CreateTestSchema),
      base::BindRepeating(MigrateTestSchema));
  EXPECT_FALSE(sql_database_->Initialize());

  auto statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, SELECT_ALL_QUERY);
  ASSERT_FALSE(statement);
}

}  // namespace
}  // namespace app_list

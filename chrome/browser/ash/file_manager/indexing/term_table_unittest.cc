// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/term_table.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("TermTableTest.db");

class TermTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<sql::Database>(sql::DatabaseOptions());
    ASSERT_TRUE(InitDb(*db_));
  }

  void TearDown() override {
    db_->Close();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  base::FilePath db_file_path() {
    return temp_dir_.GetPath().Append(kDatabaseName);
  }

  bool InitDb(sql::Database& db) {
    if (db.is_open()) {
      return true;
    }
    if (!db.Open(db_file_path())) {
      return false;
    }
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> db_;
};

TEST_F(TermTableTest, Init) {
  TermTable table(db_.get());
  EXPECT_TRUE(table.Init());
}

TEST_F(TermTableTest, GetTermId) {
  TermTable table(db_.get());
  EXPECT_TRUE(table.Init());

  EXPECT_EQ(table.GetTermId("hello"), -1);
  EXPECT_EQ(table.GetOrCreateTermId("hello"), 1);
  EXPECT_EQ(table.GetTermId("hello"), 1);
  EXPECT_EQ(table.GetOrCreateTermId("there"), 2);
  EXPECT_EQ(table.GetTermId("there"), 2);
  EXPECT_EQ(table.GetTermId("O'Neill"), -1);
  EXPECT_EQ(table.GetOrCreateTermId("O'Neill"), 3);
}

TEST_F(TermTableTest, DeleteTerm) {
  TermTable table(db_.get());
  EXPECT_TRUE(table.Init());

  EXPECT_EQ(table.DeleteTerm("hello"), -1);
  EXPECT_EQ(table.GetOrCreateTermId("hello"), 1);
  EXPECT_EQ(table.DeleteTerm("hello"), 1);
}

TEST_F(TermTableTest, GetTerm) {
  TermTable table(db_.get());
  EXPECT_TRUE(table.Init());

  std::string term;
  EXPECT_EQ(table.GetTerm(1, &term), -1);
  EXPECT_EQ(table.GetOrCreateTermId("hello"), 1);
  EXPECT_EQ(table.GetTerm(1, &term), 1);
  EXPECT_EQ(term, "hello");
  EXPECT_EQ(table.DeleteTerm("hello"), 1);
  term = "";
  EXPECT_EQ(table.GetTerm(1, &term), -1);
  EXPECT_TRUE(term.empty());
}

}  // namespace
}  // namespace file_manager

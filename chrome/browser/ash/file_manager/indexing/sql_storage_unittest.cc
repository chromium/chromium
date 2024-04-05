// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/sql_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace file_manager {
namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("SqlStorageTest.db");

class SqlStorageTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_ = std::make_unique<SqlStorage>(db_file_path(), "test_uma_tag");
  }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath db_file_path() {
    return temp_dir_.GetPath().Append(kDatabaseName);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SqlStorage> storage_;
};

TEST_F(SqlStorageTest, Init) {
  EXPECT_TRUE(storage_->Init());
}

TEST_F(SqlStorageTest, Close) {
  EXPECT_TRUE(storage_->Close());
}

TEST_F(SqlStorageTest, GetTermId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetTermId("foo", false), -1);
  EXPECT_EQ(storage_->GetTermId("foo", true), 1);
  EXPECT_EQ(storage_->GetTermId("foo", false), 1);
}

TEST_F(SqlStorageTest, DeleteTerm) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->DeleteTerm("foo"), -1);
  EXPECT_EQ(storage_->GetTermId("foo", true), 1);
  EXPECT_EQ(storage_->DeleteTerm("foo"), 1);
}

TEST_F(SqlStorageTest, GetOrCreateUrlId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  GURL url("filesystem:chrome://file-manager/external/Downloads-u123/foo.txt");
  EXPECT_EQ(storage_->GetOrCreateUrlId(url), 1);
}

TEST_F(SqlStorageTest, GetUrlId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  GURL url("filesystem:chrome://file-manager/external/Downloads-u123/foo.txt");
  EXPECT_EQ(storage_->GetUrlId(url), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(url), 1);
  EXPECT_EQ(storage_->GetUrlId(url), 1);
}

TEST_F(SqlStorageTest, DeleteUrl) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  GURL url("filesystem:chrome://file-manager/external/Downloads-u123/foo.txt");
  EXPECT_EQ(storage_->DeleteUrl(url), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(url), 1);
  EXPECT_EQ(storage_->DeleteUrl(url), 1);
}

}  // namespace
}  // namespace file_manager

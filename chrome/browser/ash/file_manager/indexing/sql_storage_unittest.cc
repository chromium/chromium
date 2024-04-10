// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/sql_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace file_manager {
namespace {

static constexpr base::Time::Exploded kTestTimeExploded = {
    .year = 2020,
    .month = 11,
    .day_of_month = 4,
};

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("SqlStorageTest.db");

class SqlStorageTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_ = std::make_unique<SqlStorage>(db_file_path(), "test_uma_tag");
    foo_url_ = GURL(
        "filesystem:chrome://file-manager/external/Downloads-u123/foo.txt");
    EXPECT_TRUE(
        base::Time::FromUTCExploded(kTestTimeExploded, &foo_modified_time_));
  }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath db_file_path() {
    return temp_dir_.GetPath().Append(kDatabaseName);
  }

 protected:
  GURL foo_url_;
  base::Time foo_modified_time_;
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

  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
}

TEST_F(SqlStorageTest, GetUrlId) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->GetUrlId(foo_url_), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
  EXPECT_EQ(storage_->GetUrlId(foo_url_), 1);
}

TEST_F(SqlStorageTest, DeleteUrl) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(storage_->DeleteUrl(foo_url_), -1);
  EXPECT_EQ(storage_->GetOrCreateUrlId(foo_url_), 1);
  EXPECT_EQ(storage_->DeleteUrl(foo_url_), 1);
}

TEST_F(SqlStorageTest, GetFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  FileInfo got_file_info(GURL(""), 0, base::Time::Now());

  EXPECT_EQ(-1, storage_->GetFileInfo(foo_url_, &got_file_info));

  FileInfo put_file_info(foo_url_, 100, base::Time());
  EXPECT_EQ(1, storage_->GetOrCreateUrlId(foo_url_));
  EXPECT_EQ(1, storage_->PutFileInfo(put_file_info));
  EXPECT_EQ(1, storage_->GetFileInfo(foo_url_, &got_file_info));
  EXPECT_EQ(put_file_info.file_url, got_file_info.file_url);
  EXPECT_EQ(put_file_info.last_modified, got_file_info.last_modified);
  EXPECT_EQ(put_file_info.size, got_file_info.size);

  put_file_info.size = put_file_info.size + 100;
  EXPECT_EQ(1, storage_->PutFileInfo(put_file_info));
  EXPECT_EQ(1, storage_->GetFileInfo(foo_url_, &got_file_info));
  EXPECT_EQ(put_file_info.size, got_file_info.size);
}

TEST_F(SqlStorageTest, PutFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  FileInfo file_info(foo_url_, 100, base::Time());
  // If the URL is unknown, expect put operation to fail.
  EXPECT_EQ(-1, storage_->PutFileInfo(file_info));
  // Store the URL first and expect the operation to succeed.
  EXPECT_EQ(1, storage_->GetOrCreateUrlId(file_info.file_url));
  EXPECT_EQ(1, storage_->PutFileInfo(file_info));
}

TEST_F(SqlStorageTest, DeleteFileInfo) {
  // Must initialize before use.
  ASSERT_TRUE(storage_->Init());

  EXPECT_EQ(-1, storage_->DeleteFileInfo(foo_url_));

  FileInfo put_file_info(foo_url_, 100, base::Time());
  EXPECT_EQ(1, storage_->GetOrCreateUrlId(foo_url_));
  EXPECT_EQ(1, storage_->PutFileInfo(put_file_info));
  EXPECT_EQ(1, storage_->DeleteFileInfo(foo_url_));
}
}  // namespace
}  // namespace file_manager

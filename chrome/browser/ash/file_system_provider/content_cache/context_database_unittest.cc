// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

class FileSystemProviderContextDatabaseTest : public testing::Test {
 protected:
  FileSystemProviderContextDatabaseTest() = default;
  ~FileSystemProviderContextDatabaseTest() override = default;

  void SetUp() override { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileSystemProviderContextDatabaseTest, DbCreatedOnInitialize) {
  base::FilePath db_path = temp_dir_.GetPath().Append("context.db");
  ContextDatabase context_db(db_path);
  EXPECT_TRUE(context_db.Initialize());
  EXPECT_TRUE(base::PathExists(db_path));
}

}  // namespace
}  // namespace ash::file_system_provider

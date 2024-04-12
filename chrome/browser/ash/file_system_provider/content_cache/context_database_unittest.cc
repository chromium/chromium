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
  ContextDatabase db(db_path);
  EXPECT_TRUE(db.Initialize());
  EXPECT_TRUE(base::PathExists(db_path));
}

TEST_F(FileSystemProviderContextDatabaseTest, AddItem) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Doesn't accept empty parameters;
  int64_t inserted_id = -1;
  EXPECT_FALSE(db->AddItem(base::FilePath(), "versionA", base::Time::Now(),
                           &inserted_id));
  EXPECT_FALSE(db->AddItem(base::FilePath("/fsp_path.txt"), "",
                           base::Time::Now(), &inserted_id));
  EXPECT_FALSE(db->AddItem(base::FilePath("/fsp_path.txt"), "versionA",
                           base::Time(), &inserted_id));
  EXPECT_EQ(inserted_id, -1);

  // Item added returns an auto incremented ID.
  EXPECT_TRUE(db->AddItem(base::FilePath("/fsp_path.txt"), "versionA",
                          base::Time::Now(), &inserted_id));
  EXPECT_EQ(inserted_id, 1);
  EXPECT_TRUE(db->AddItem(base::FilePath("/fsp_path_1.txt"), "versionA",
                          base::Time::Now(), &inserted_id));
  EXPECT_EQ(inserted_id, 2);

  // If an item is added that matches the UNIQUE(fsp_path, version_tag)
  // constraint the new ID is returned and the old ID gets removed.
  EXPECT_TRUE(db->AddItem(base::FilePath("/fsp_path.txt"), "versionA",
                          base::Time::Now(), &inserted_id));
  EXPECT_EQ(inserted_id, 3);

  // Expect that the item with id 1 is no longer available.
  ContextDatabase::Item item;
  db->GetItemById(1, item);
  EXPECT_FALSE(item.item_exists);
}

TEST_F(FileSystemProviderContextDatabaseTest, GetItemById) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Negative IDs should fail.
  ContextDatabase::Item item;
  EXPECT_FALSE(db->GetItemById(-1, item));

  // Insert an item into the database.
  int64_t inserted_id = -1;
  base::FilePath fsp_path("/fsp_path.txt");
  std::string version_tag("versionA");
  base::Time accessed_time = base::Time::Now();
  EXPECT_TRUE(db->AddItem(fsp_path, version_tag, accessed_time, &inserted_id));

  // Retrieve the item back from the database.
  EXPECT_TRUE(db->GetItemById(inserted_id, item));
  EXPECT_TRUE(item.item_exists);

  // We store the time in ms since unix epoch which doesn't have the same
  // granularity as the `EXPECT_EQ` comparison, convert back to ensure the
  // values are the same.
  EXPECT_EQ(item.accessed_time.InMillisecondsSinceUnixEpoch(),
            accessed_time.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(item.fsp_path, fsp_path);
  EXPECT_EQ(item.version_tag, version_tag);
}

TEST_F(FileSystemProviderContextDatabaseTest, UpdateAccessedTime) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Insert an item into the database.
  int64_t inserted_id = -1;
  base::FilePath fsp_path("/fsp_path.txt");
  std::string version_tag("versionA");
  base::Time accessed_time = base::Time::Now();
  EXPECT_TRUE(db->AddItem(fsp_path, version_tag, accessed_time, &inserted_id));

  base::Time new_accessed_time;
  EXPECT_TRUE(
      base::Time::FromString("1 Jun 2021 10:00 GMT", &new_accessed_time));
  EXPECT_TRUE(db->UpdateAccessedTime(inserted_id, new_accessed_time));

  // Retrieve the item back from the database.
  ContextDatabase::Item item;
  EXPECT_TRUE(db->GetItemById(inserted_id, item));
  EXPECT_TRUE(item.item_exists);
  EXPECT_EQ(item.accessed_time.InMillisecondsSinceUnixEpoch(),
            new_accessed_time.InMillisecondsSinceUnixEpoch());
}

}  // namespace
}  // namespace ash::file_system_provider

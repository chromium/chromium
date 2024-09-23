// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

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
  std::unique_ptr<std::optional<ContextDatabase::Item>> item =
      db->GetItemById(1);
  EXPECT_TRUE(item);
  EXPECT_FALSE(item->has_value());
}

TEST_F(FileSystemProviderContextDatabaseTest, GetItemById) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Negative IDs should fail.
  EXPECT_FALSE(db->GetItemById(-1));

  // Insert an item into the database.
  int64_t inserted_id = -1;
  base::FilePath fsp_path("/fsp_path.txt");
  std::string version_tag("versionA");
  base::Time accessed_time = base::Time::Now();
  EXPECT_TRUE(db->AddItem(fsp_path, version_tag, accessed_time, &inserted_id));

  // Retrieve the item back from the database.
  std::unique_ptr<std::optional<ContextDatabase::Item>> item =
      db->GetItemById(inserted_id);
  EXPECT_TRUE(item);
  EXPECT_TRUE(item->has_value());

  // We store the time in ms since unix epoch which doesn't have the same
  // granularity as the `EXPECT_EQ` comparison, convert back to ensure the
  // values are the same.
  EXPECT_EQ(item->value().accessed_time.InMillisecondsSinceUnixEpoch(),
            accessed_time.InMillisecondsSinceUnixEpoch());
  EXPECT_EQ(item->value().fsp_path, fsp_path);
  EXPECT_EQ(item->value().version_tag, version_tag);
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
  std::unique_ptr<std::optional<ContextDatabase::Item>> item =
      db->GetItemById(inserted_id);
  EXPECT_TRUE(item);
  EXPECT_TRUE(item->has_value());
  EXPECT_EQ(item->value().accessed_time.InMillisecondsSinceUnixEpoch(),
            new_accessed_time.InMillisecondsSinceUnixEpoch());
}

TEST_F(FileSystemProviderContextDatabaseTest, GetAllItems) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Insert 2 items into the database.
  int64_t first_item_id = -1;
  int64_t second_item_id = -1;
  EXPECT_TRUE(db->AddItem(base::FilePath("/a.txt"), "versionA",
                          base::Time::Now(), &first_item_id));
  EXPECT_TRUE(db->AddItem(base::FilePath("/b.txt"), "versionB",
                          base::Time::Now(), &second_item_id));

  // Ensure the items exist.
  using Item = ContextDatabase::Item;
  EXPECT_THAT(db->GetAllItems(),
              UnorderedElementsAre(
                  Pair(first_item_id,
                       AllOf(Field(&Item::fsp_path, base::FilePath("/a.txt")),
                             Field(&Item::version_tag, "versionA"))),
                  Pair(second_item_id,
                       AllOf(Field(&Item::fsp_path, base::FilePath("/b.txt")),
                             Field(&Item::version_tag, "versionB")))));
}

TEST_F(FileSystemProviderContextDatabaseTest, RemoveItemsByIds) {
  std::unique_ptr<ContextDatabase> db =
      std::make_unique<ContextDatabase>(base::FilePath());
  EXPECT_TRUE(db->Initialize());

  // Insert 2 items into the database.
  int64_t first_item_id = -1;
  int64_t second_item_id = -1;
  EXPECT_TRUE(db->AddItem(base::FilePath("/a.txt"), "versionA",
                          base::Time::Now(), &first_item_id));
  EXPECT_TRUE(db->AddItem(base::FilePath("/b.txt"), "versionA",
                          base::Time::Now(), &second_item_id));

  // Remove both items.
  EXPECT_TRUE(db->RemoveItemsByIds({first_item_id, second_item_id}));

  // Ensure the items don't exist.
  EXPECT_THAT(db->GetAllItems(), IsEmpty());
}

}  // namespace
}  // namespace ash::file_system_provider

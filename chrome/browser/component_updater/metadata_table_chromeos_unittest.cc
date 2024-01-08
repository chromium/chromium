// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/metadata_table_chromeos.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kMetadataContentItemHashedUserIdKey[] = "hashed_user_id";
constexpr char kMetadataContentItemComponentKey[] = "component";
constexpr char kHashedUserId[2][6] = {"user1", "user2"};
constexpr char kComponent[2][11] = {"component1", "component2"};

}  // namespace

class CrOSComponentInstallerMetadataTest : public testing::Test {
 public:
  CrOSComponentInstallerMetadataTest() = default;

  CrOSComponentInstallerMetadataTest(
      const CrOSComponentInstallerMetadataTest&) = delete;
  CrOSComponentInstallerMetadataTest& operator=(
      const CrOSComponentInstallerMetadataTest&) = delete;

  ~CrOSComponentInstallerMetadataTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CrOSComponentInstallerMetadataTest, Add) {
  // Initialize protobuf object.
  std::unique_ptr<MetadataTable> metadata_table =
      MetadataTable::CreateForTest();

  // Add component items.
  metadata_table->AddItem(kHashedUserId[0], kComponent[0]);
  metadata_table->AddItem(kHashedUserId[1], kComponent[1]);
  metadata_table->AddItem(kHashedUserId[0], kComponent[1]);
  ASSERT_EQ(metadata_table->installed_items_.size(), 3U);
  EXPECT_TRUE(
      metadata_table->HasComponentForUser(kHashedUserId[0], kComponent[0]));
  EXPECT_TRUE(
      metadata_table->HasComponentForUser(kHashedUserId[1], kComponent[1]));
  EXPECT_TRUE(
      metadata_table->HasComponentForUser(kHashedUserId[0], kComponent[1]));
  EXPECT_FALSE(
      metadata_table->HasComponentForUser(kHashedUserId[1], kComponent[0]));
  // Add an existing item.
  metadata_table->AddItem(kHashedUserId[0], kComponent[0]);
  EXPECT_EQ(metadata_table->installed_items_.size(), 3U);
}

TEST_F(CrOSComponentInstallerMetadataTest, Delete) {
  // Initialize protobuf object.
  std::unique_ptr<MetadataTable> metadata_table =
      MetadataTable::CreateForTest();

  // Add component items.
  metadata_table->AddItem(kHashedUserId[0], kComponent[0]);
  metadata_table->AddItem(kHashedUserId[1], kComponent[1]);
  metadata_table->AddItem(kHashedUserId[0], kComponent[1]);

  // Delete a component item that does not exist.
  EXPECT_FALSE(metadata_table->DeleteItem(kHashedUserId[1], kComponent[0]));

  // Delete component items.
  EXPECT_TRUE(metadata_table->DeleteItem(kHashedUserId[1], kComponent[1]));
  EXPECT_TRUE(metadata_table->DeleteItem(kHashedUserId[0], kComponent[1]));
  ASSERT_EQ(metadata_table->installed_items_.size(), 1U);
  const base::Value& item = metadata_table->installed_items_[0];
  EXPECT_EQ(*item.GetDict().FindString(kMetadataContentItemHashedUserIdKey),
            kHashedUserId[0]);
  EXPECT_EQ(*item.GetDict().FindString(kMetadataContentItemComponentKey),
            kComponent[0]);
}

}  // namespace component_updater

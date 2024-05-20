// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_store.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

// Tests blocked app registry.
class BlockedAppStoreTest : public testing::Test {
 public:
  BlockedAppStoreTest() = default;
  BlockedAppStoreTest(const BlockedAppStoreTest&) = delete;
  BlockedAppStoreTest& operator=(const BlockedAppStoreTest&) = delete;

  ~BlockedAppStoreTest() override = default;

 protected:
  BlockedAppStore* store() { return store_.get(); }

  // testing::Test:
  void SetUp() override;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::unique_ptr<BlockedAppStore> store_;
};

void BlockedAppStoreTest::SetUp() {
  testing::Test::SetUp();

  store_ = std::make_unique<BlockedAppStore>(profile_.GetPrefs());
}

// Tests reading from the empty preference.
TEST_F(BlockedAppStoreTest, TestGetFromEmptyPref) {
  BlockedAppMap apps = store()->GetFromPref();

  EXPECT_EQ(0UL, apps.size());
}

// Tests writing and reading one blocked app.
TEST_F(BlockedAppStoreTest, TestBlockedApp) {
  BlockedAppMap apps_in;

  const std::string app_id = "abc";
  const base::Time timestamp = base::Time::Now();
  apps_in[app_id] = BlockedAppDetails(timestamp);

  store()->SaveToPref(apps_in);

  const BlockedAppMap apps_out = store()->GetFromPref();
  EXPECT_EQ(1UL, apps_out.size());
  ASSERT_TRUE(base::Contains(apps_out, app_id));
  EXPECT_EQ(timestamp, apps_out.at(app_id).block_timestamp());
  EXPECT_EQ(std::nullopt, apps_out.at(app_id).uninstall_timestamp());
}

// Tests writing and reading one uninstalled app.
TEST_F(BlockedAppStoreTest, TestUninstalledApp) {
  BlockedAppMap apps_in;

  const std::string app_id = "abc";
  const base::Time block_timestamp = base::Time::Now();
  const base::Time uninstall_timestamp = base::Time::Now() + base::Minutes(5);
  apps_in[app_id] = BlockedAppDetails(block_timestamp, uninstall_timestamp);

  store()->SaveToPref(apps_in);

  const BlockedAppMap apps_out = store()->GetFromPref();
  EXPECT_EQ(1UL, apps_out.size());
  ASSERT_TRUE(base::Contains(apps_out, app_id));
  EXPECT_EQ(block_timestamp, apps_out.at(app_id).block_timestamp());
  EXPECT_EQ(uninstall_timestamp, *apps_out.at(app_id).uninstall_timestamp());
}

// Tests writing and reading multiple apps.
TEST_F(BlockedAppStoreTest, TestMultipleApps) {
  const std::string base_app_id = "abc";
  const base::Time initial_time = base::Time::Now();
  const base::TimeDelta uninstall_delta = base::Hours(5);
  const base::TimeDelta block_delta = base::Minutes(13);
  const size_t apps_count = 10;

  BlockedAppMap apps_in;
  base::Time timestamp = initial_time;
  // Adds apps with the different block and uninstall timestamps.
  for (size_t i = 0; i < apps_count; ++i) {
    const std::string app_id =
        base::StrCat({base_app_id, base::NumberToString(i)});
    apps_in[app_id] = BlockedAppDetails(timestamp, timestamp + uninstall_delta);
    timestamp += block_delta;
  }

  store()->SaveToPref(apps_in);

  const BlockedAppMap apps_out = store()->GetFromPref();
  EXPECT_EQ(apps_count, apps_out.size());

  for (size_t i = 0; i < apps_count; ++i) {
    const std::string app_id =
        base::StrCat({base_app_id, base::NumberToString(i)});
    ASSERT_TRUE(base::Contains(apps_out, app_id));

    const BlockedAppDetails& details = apps_out.at(app_id);

    const base::Time expected_block_time = initial_time + i * block_delta;
    EXPECT_EQ(expected_block_time, details.block_timestamp());

    const base::Time expected_uninstall_time =
        expected_block_time + uninstall_delta;
    EXPECT_EQ(expected_uninstall_time, *details.uninstall_timestamp());
  }
}

}  // namespace ash::on_device_controls

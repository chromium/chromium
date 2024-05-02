// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

// Tests blocked app registry.
class BlockedAppRegistryTest : public testing::Test {
 protected:
  BlockedAppRegistryTest() = default;
  BlockedAppRegistryTest(const BlockedAppRegistryTest&) = delete;
  BlockedAppRegistryTest& operator=(const BlockedAppRegistryTest&) = delete;
  ~BlockedAppRegistryTest() override = default;

  BlockedAppRegistry& registry() { return registry_; }

 private:
  BlockedAppRegistry registry_;
};

TEST_F(BlockedAppRegistryTest, AddApp) {
  const std::vector<std::string> app_ids = {"abc", "def"};

  EXPECT_EQ(0UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[1]));

  registry().AddApp(app_ids[0]);
  EXPECT_EQ(1UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[1]));

  registry().AddApp(app_ids[1]);
  EXPECT_EQ(2UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[1]));

  std::set<std::string> blocked_apps = registry().GetBlockedApps();
  for (const auto& app_id : app_ids) {
    EXPECT_TRUE(base::Contains(blocked_apps, app_id));
  }
}

TEST_F(BlockedAppRegistryTest, RemoveApp) {
  const std::vector<std::string> app_ids = {"abc", "def"};

  registry().AddApp(app_ids[0]);
  registry().AddApp(app_ids[1]);
  EXPECT_EQ(2UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[1]));

  registry().RemoveApp(app_ids[0]);
  EXPECT_EQ(1UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry().GetAppState(app_ids[1]));

  registry().RemoveApp(app_ids[1]);
  EXPECT_EQ(0UL, registry().GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry().GetAppState(app_ids[1]));
}

}  // namespace ash::on_device_controls

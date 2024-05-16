// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

// Tests blocked app registry.
class BlockedAppRegistryTest : public AppControlsTestBase {
 public:
  BlockedAppRegistryTest() = default;
  ~BlockedAppRegistryTest() override = default;

  BlockedAppRegistry* registry() { return registry_.get(); }

  // Returns app readiness or nullopt if the app with `app_id` was not found in
  // the app service cache.
  std::optional<apps::Readiness> GetAppReadiness(const std::string& app_id);

 protected:
  // AppControlsTestBase:
  void SetUp() override;

 private:
  std::unique_ptr<BlockedAppRegistry> registry_;
};

std::optional<apps::Readiness> BlockedAppRegistryTest::GetAppReadiness(
    const std::string& app_id) {
  apps::AppRegistryCache& app_cache =
      app_service_test().proxy()->AppRegistryCache();
  std::optional<apps::Readiness> readiness;
  app_cache.ForOneApp(app_id, [&readiness](const apps::AppUpdate& update) {
    readiness = update.Readiness();
  });
  return readiness;
}

void BlockedAppRegistryTest::SetUp() {
  AppControlsTestBase::SetUp();

  registry_ = std::make_unique<BlockedAppRegistry>(app_service_test().proxy(),
                                                   profile().GetPrefs());
}

// Tests registry state when adding an app.
TEST_F(BlockedAppRegistryTest, AddApp) {
  const std::vector<std::string> app_ids = {"abc", "def"};

  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[1]));

  registry()->AddApp(app_ids[0]);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[1]));

  registry()->AddApp(app_ids[1]);
  EXPECT_EQ(2UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[1]));

  std::set<std::string> blocked_apps = registry()->GetBlockedApps();
  for (const auto& app_id : app_ids) {
    EXPECT_TRUE(base::Contains(blocked_apps, app_id));
  }
}

// Tests registry state when removing an app.
TEST_F(BlockedAppRegistryTest, RemoveApp) {
  const std::vector<std::string> app_ids = {"abc", "def"};

  registry()->AddApp(app_ids[0]);
  registry()->AddApp(app_ids[1]);
  EXPECT_EQ(2UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[1]));

  registry()->RemoveApp(app_ids[0]);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[1]));

  registry()->RemoveApp(app_ids[1]);
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[1]));
}

// Tests that available app is not blocked upon installation and reinstallation.
TEST_F(BlockedAppRegistryTest, ReinstallAvailableApp) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(apps::Readiness::kUninstalledByUser, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Assuming the same AppService id will be generated upon reinstallation.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));
}

// Tests that blocked app gets blocked upon reinstallation.
TEST_F(BlockedAppRegistryTest, ReinstallBlockedApp) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Simulate app being blocked.
  registry()->AddApp(app_id);
  EXPECT_EQ(apps::Readiness::kDisabledByLocalSettings, GetAppReadiness(app_id));
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(apps::Readiness::kUninstalledByUser, GetAppReadiness(app_id));
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlockedUninstalled,
            registry()->GetAppState(app_id));

  // Assuming the same AppService id will be generated upon reinstallation.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(apps::Readiness::kDisabledByLocalSettings, GetAppReadiness(app_id));
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));
}

// Tests that blocked app gets blocked upon state restoration.
TEST_F(BlockedAppRegistryTest, RestoreBlockedApp) {
  // Install and uninstall app to get app id.
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(apps::Readiness::kUninstalledByUser, GetAppReadiness(app_id));
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Simulate app being blocked.
  registry()->AddApp(app_id);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));

  // Simulate app being restored - same path as installed.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(apps::Readiness::kDisabledByLocalSettings, GetAppReadiness(app_id));
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));
}

// Tests that app state gets updated when added or removed from the
// blocked app registry.
TEST_F(BlockedAppRegistryTest, BlockAndUnblock) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));

  registry()->AddApp(app_id);
  EXPECT_EQ(apps::Readiness::kDisabledByLocalSettings, GetAppReadiness(app_id));

  registry()->RemoveApp(app_id);
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));
}

}  // namespace ash::on_device_controls

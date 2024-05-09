// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/child_accounts/apps/app_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

// Tests blocked app registry.
class BlockedAppRegistryTest : public testing::Test {
 protected:
  BlockedAppRegistryTest() = default;
  BlockedAppRegistryTest(const BlockedAppRegistryTest&) = delete;
  BlockedAppRegistryTest& operator=(const BlockedAppRegistryTest&) = delete;
  ~BlockedAppRegistryTest() override = default;

  BlockedAppRegistry* registry() { return registry_.get(); }
  ArcAppTest& arc_test() { return arc_test_; }

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Installs ARC++ app with the given `package_name` and returns its AppService
  // id.
  std::string InstallArcApp(const std::string& package_name,
                            const std::string& app_name);
  void UninstallArcApp(const std::string& package_name);

  //  private:
  base::test::ScopedCommandLine scoped_command_line_;
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  ArcAppTest arc_test_;

  std::unique_ptr<BlockedAppRegistry> registry_;
};

void BlockedAppRegistryTest::SetUp() {
  testing::Test::SetUp();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDefaultApps);

  app_service_test_.SetUp(&profile_);
  arc_test_.SetUp(&profile_);
  task_environment_.RunUntilIdle();

  registry_ = std::make_unique<BlockedAppRegistry>(app_service_test_.proxy(),
                                                   profile_.GetPrefs());
}

void BlockedAppRegistryTest::TearDown() {
  arc_test_.TearDown();

  testing::Test::TearDown();
}

std::string BlockedAppRegistryTest::InstallArcApp(
    const std::string& package_name,
    const std::string& app_name) {
  arc_test_.AddPackage(CreateArcAppPackage(package_name)->Clone());
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(CreateArcAppInfo(package_name, app_name));
  arc_test_.app_instance()->SendPackageAppListRefreshed(package_name, apps);
  task_environment_.RunUntilIdle();

  return arc::ArcPackageNameToAppId(package_name, &profile_);
}

void BlockedAppRegistryTest::UninstallArcApp(const std::string& package_name) {
  arc_test_.app_instance()->UninstallPackage(package_name);
  task_environment_.RunUntilIdle();
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
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Assuming the same AppService id will be generated upon reinstallation.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));
}

// Tests that blocked app gets blocked upon reinstallation.
TEST_F(BlockedAppRegistryTest, ReinstallBlockedApp) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Simulate app being blocked.
  registry()->AddApp(app_id);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlockedUninstalled,
            registry()->GetAppState(app_id));

  // Assuming the same AppService id will be generated upon reinstallation.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));
}

// Tests that blocked app gets blocked upon state restoration.
TEST_F(BlockedAppRegistryTest, RestoreBlockedApp) {
  // Install and uninstall app to get app id.
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  ASSERT_FALSE(app_id.empty());
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  UninstallArcApp(package_name);
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_id));

  // Simulate app being blocked.
  registry()->AddApp(app_id);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));

  // Simulate app being restored - same path as installed.
  InstallArcApp(package_name, app_name);
  EXPECT_EQ(1UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));
}

}  // namespace ash::on_device_controls

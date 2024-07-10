// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app_controls_metrics_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_metrics_utils.h"
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

  void set_registry(std::unique_ptr<BlockedAppRegistry> registry) {
    registry_ = std::move(registry);
  }

  // Returns app readiness or nullopt if the app with `app_id` was not found in
  // the app service cache.
  std::optional<apps::Readiness> GetAppReadiness(const std::string& app_id);

 protected:
  // AppControlsTestBase:
  void SetUp() override;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<BlockedAppRegistry> registry_;
  base::HistogramTester histogram_tester_;
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

// Tests registry state and histograms when removing all apps.
TEST_F(BlockedAppRegistryTest, RemoveAllAppsAndHistograms) {
  const std::vector<std::string> app_ids = {"abc", "def", "ghi"};

  registry()->AddApp(app_ids[0]);
  registry()->AddApp(app_ids[1]);
  registry()->AddApp(app_ids[2]);
  EXPECT_EQ(3UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[1]));
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_ids[2]));

  registry()->RemoveAllApps();
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/0, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kUnblockAllApps, 1);
  histogram_tester().ExpectTotalCount(
      kOnDeviceControlsBlockAppActionHistogramName, 4);
  EXPECT_EQ(0UL, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[0]));
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[1]));
  EXPECT_EQ(LocalAppState::kAvailable, registry()->GetAppState(app_ids[2]));
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

// Tests that uninstalling more than the max number of allowed blocked apps
// removes the oldest uninstalled app from the registry.
TEST_F(BlockedAppRegistryTest, UninstallMaxBlockedApps) {
  // Register the max number of uninstalled blocked apps
  const size_t apps_count = 100;
  std::vector<std::string> all_blocked_apps;
  for (size_t i = 0; i < apps_count; ++i) {
    std::string app_name = base::StrCat({"app_name", base::NumberToString(i)});
    std::string package_name =
        base::StrCat({"com.example.app", base::NumberToString(i)});
    std::string app_id = InstallArcApp(package_name, app_name);
    all_blocked_apps.push_back(app_id);

    registry()->AddApp(app_id);
    EXPECT_EQ(apps::Readiness::kDisabledByLocalSettings,
              GetAppReadiness(app_id));
    EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(app_id));

    UninstallArcApp(package_name);
  }

  EXPECT_EQ(apps_count, registry()->GetBlockedApps().size());

  const std::string new_app_package = "com.example.new_app";
  const std::string new_app_id = InstallArcApp(new_app_package, "new_app_name");
  registry()->AddApp(new_app_id);
  EXPECT_EQ(apps_count + 1, registry()->GetBlockedApps().size());
  EXPECT_EQ(LocalAppState::kBlocked, registry()->GetAppState(new_app_id));

  UninstallArcApp(new_app_package);

  // Check the first uninstalled app added was removed from the registry.
  const std::string oldest_uninstalled_app = all_blocked_apps[0];
  EXPECT_EQ(apps_count, registry()->GetBlockedApps().size());
  ASSERT_FALSE(registry()->GetBlockedApps().contains(oldest_uninstalled_app));
  EXPECT_EQ(apps::Readiness::kUninstalledByUser,
            GetAppReadiness(oldest_uninstalled_app));
  EXPECT_EQ(LocalAppState::kAvailable,
            registry()->GetAppState(oldest_uninstalled_app));
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsAppRemovalHistogramName,
      OnDeviceControlsAppRemoval::kOldestUninstalledAppRemoved, 1);
}

TEST_F(BlockedAppRegistryTest, TestHistogramsOnBlockAndUnblockApp) {
  histogram_tester().ExpectUniqueSample(
      kOnDeviceControlsBlockedAppsEngagementHistogramName, /*sample=*/0, 1);
  histogram_tester().ExpectUniqueSample(
      kOnDeviceControlsPinSetCompletedHistogramName,
      /*sample=*/0, 1);

  const std::vector<std::string> app_ids = {"abc", "def"};

  registry()->AddApp(app_ids[0]);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/1, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kBlockApp, 1);

  registry()->AddApp(app_ids[1]);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/2, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kBlockApp, 2);

  registry()->RemoveApp(app_ids[0]);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/1, 2);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kUnblockApp, 1);
  histogram_tester().ExpectTotalCount(
      kOnDeviceControlsBlockAppActionHistogramName, 3);
}

TEST_F(BlockedAppRegistryTest, TestHistogramsOnInitialization) {
  profile().GetPrefs()->SetBoolean(prefs::kOnDeviceAppControlsSetupCompleted,
                                   true);
  const std::vector<std::string> app_ids = {"abc", "def"};
  registry()->AddApp(app_ids[0]);
  registry()->AddApp(app_ids[1]);

  // Create new registry and histogram counter to simulate a new user session.
  base::HistogramTester histogram_tester;
  set_registry(std::make_unique<BlockedAppRegistry>(app_service_test().proxy(),
                                                    profile().GetPrefs()));

  histogram_tester.ExpectUniqueSample(
      kOnDeviceControlsBlockedAppsEngagementHistogramName, /*sample=*/2, 1);
  histogram_tester.ExpectUniqueSample(
      kOnDeviceControlsPinSetCompletedHistogramName,
      /*sample=*/1, 1);
}

TEST_F(BlockedAppRegistryTest, TestHistogramOnUninstallBlockedApp) {
  const std::string package_name = "com.example.app1", app_name = "app1";
  const std::string app_id = InstallArcApp(package_name, app_name);
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(app_id));

  registry()->AddApp(app_id);
  histogram_tester().ExpectUniqueSample(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/1, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kBlockApp, 1);

  UninstallArcApp(package_name);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kUninstallBlockedApp, 1);
  histogram_tester().ExpectTotalCount(
      kOnDeviceControlsBlockAppActionHistogramName, 2);
}

TEST_F(BlockedAppRegistryTest, TestHistogramOnBlockAndUnblockErrors) {
  const std::string app_id = "abc";

  registry()->AddApp(app_id);
  histogram_tester().ExpectUniqueSample(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/1, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kBlockApp, 1);

  // Try to add an app that is already added.
  registry()->AddApp(app_id);
  histogram_tester().ExpectUniqueSample(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/1, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kBlockAppError, 1);

  registry()->RemoveApp(app_id);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockedAppsCountHistogramName, /*sample=*/0, 1);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kUnblockApp, 1);

  // Try to unblock an app that is not blocked.
  registry()->RemoveApp(app_id);
  histogram_tester().ExpectBucketCount(
      kOnDeviceControlsBlockAppActionHistogramName,
      OnDeviceControlsBlockAppAction::kUnblockAppError, 1);
  histogram_tester().ExpectTotalCount(
      kOnDeviceControlsBlockAppActionHistogramName, 4);
}

}  // namespace ash::on_device_controls

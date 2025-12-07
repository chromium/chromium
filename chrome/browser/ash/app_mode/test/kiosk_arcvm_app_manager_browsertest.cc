// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia_rep_default.h"

namespace ash {

namespace {

// The purpose of this waiter - wait for being notified some amount of times.
class NotificationWaiter : public KioskAppManagerObserver {
 public:
  // In constructor we provide instance of KioskArcvmAppManager and subscribe
  // for notifications from it, and minimum amount of times we expect to get the
  // notification.
  explicit NotificationWaiter(KioskArcvmAppManager* manager)
      : manager_(manager) {
    kiosk_app_manager_observation_.Observe(manager_);
  }
  NotificationWaiter(const NotificationWaiter&) = delete;
  NotificationWaiter& operator=(const NotificationWaiter&) = delete;
  ~NotificationWaiter() override = default;

  void Wait(int times) {
    for (int count = 0; count < times; count++) {
      EXPECT_TRUE(notifications_received_.Take());
    }
  }

 private:
  // KioskAppManagerObserver:
  void OnKioskAppsSettingsChanged() override {
    notifications_received_.AddValue(true);
  }

  base::test::RepeatingTestFuture<bool> notifications_received_;
  raw_ptr<KioskArcvmAppManager> manager_;
  base::ScopedObservation<KioskAppManagerBase, KioskAppManagerObserver>
      kiosk_app_manager_observation_{this};
};

std::string GenerateAccountId(std::string package_name) {
  return package_name + "@arcvm-kiosk-app";
}

}  // namespace

class KioskArcvmAppManagerTest : public InProcessBrowserTest {
 public:
  KioskArcvmAppManagerTest() : settings_helper_(false) {
    feature_list_.InitAndEnableFeature(features::kHeliumArcvmKiosk);
  }
  KioskArcvmAppManagerTest(const KioskArcvmAppManagerTest&) = delete;
  KioskArcvmAppManagerTest& operator=(const KioskArcvmAppManagerTest&) = delete;
  ~KioskArcvmAppManagerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    // TODO(crbug.com/418900186): Setup policy properly for this test.
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(browser()->profile());
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    settings_helper_.RestoreRealDeviceSettingsProvider();
  }

  void SetApps(const std::vector<policy::ArcvmKioskAppBasicInfo>& apps,
               const std::string& auto_login_account) {
    base::Value::List device_local_accounts;
    for (const policy::ArcvmKioskAppBasicInfo& app : apps) {
      device_local_accounts.Append(
          base::Value::Dict()
              .Set(kAccountsPrefDeviceLocalAccountsKeyId,
                   GenerateAccountId(app.package_name()))
              .Set(kAccountsPrefDeviceLocalAccountsKeyType,
                   static_cast<int>(
                       policy::DeviceLocalAccountType::kArcvmKioskApp))
              .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                   static_cast<int>(
                       policy::DeviceLocalAccount::EphemeralMode::kUnset))
              .Set(kAccountsPrefDeviceLocalAccountsKeyArcvmKioskPackage,
                   app.package_name())
              .Set(kAccountsPrefDeviceLocalAccountsKeyArcvmKioskClass,
                   app.class_name())
              .Set(kAccountsPrefDeviceLocalAccountsKeyArcvmKioskAction,
                   app.action())
              .Set(kAccountsPrefDeviceLocalAccountsKeyArcvmKioskDisplayName,
                   app.display_name()));
    }
    owner_settings_service_->Set(kAccountsPrefDeviceLocalAccounts,
                                 base::Value(std::move(device_local_accounts)));

    if (!auto_login_account.empty()) {
      owner_settings_service_->SetString(
          kAccountsPrefDeviceLocalAccountAutoLoginId, auto_login_account);
    }
  }

  void CleanApps() {
    base::Value device_local_accounts(base::Value::Type::LIST);
    owner_settings_service_->Set(kAccountsPrefDeviceLocalAccounts,
                                 device_local_accounts);
  }

  KioskArcvmAppManager* manager() const { return KioskArcvmAppManager::Get(); }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;
};

IN_PROC_BROWSER_TEST_F(KioskArcvmAppManagerTest, Basic) {
  policy::ArcvmKioskAppBasicInfo app1("com.package.name1", "", "", "");
  policy::ArcvmKioskAppBasicInfo app2("com.package.name2", "", "",
                                      "display name");
  std::vector<policy::ArcvmKioskAppBasicInfo> init_apps{app1, app2};

  // Set initial list of apps.
  {
    // Observer must be notified once: app list was updated.
    NotificationWaiter waiter(manager());
    SetApps(init_apps, std::string());
    waiter.Wait(1);

    std::vector<const KioskArcvmAppData*> apps = manager()->GetAppsForTesting();
    ASSERT_EQ(2u, apps.size());
    ASSERT_EQ(app1.package_name(), apps[0]->package_name());
    ASSERT_EQ(app2.package_name(), apps[1]->package_name());
    ASSERT_EQ(app1.package_name(), apps[0]->name());
    ASSERT_EQ(app2.display_name(), apps[1]->name());
    EXPECT_FALSE(manager()->GetAutoLaunchAccountId().is_valid());
    EXPECT_FALSE(manager()->current_app_was_auto_launched_with_zero_delay());
    CleanApps();
  }

  // Set auto-launch app.
  {
    // Observer must be notified twice: for policy list update and for
    // auto-launch app update.
    NotificationWaiter waiter(manager());
    SetApps(init_apps, GenerateAccountId(app2.package_name()));
    waiter.Wait(2);

    EXPECT_TRUE(manager()->GetAutoLaunchAccountId().is_valid());

    std::vector<const KioskArcvmAppData*> apps = manager()->GetAppsForTesting();
    ASSERT_EQ(2u, apps.size());
    ASSERT_EQ(app1.package_name(), apps[0]->package_name());
    ASSERT_EQ(app2.package_name(), apps[1]->package_name());
    ASSERT_EQ(app1.package_name(), apps[0]->name());
    ASSERT_EQ(app2.display_name(), apps[1]->name());
    EXPECT_TRUE(manager()->GetAutoLaunchAccountId().is_valid());
    ASSERT_EQ(apps[1]->account_id(), manager()->GetAutoLaunchAccountId());
    EXPECT_TRUE(manager()->current_app_was_auto_launched_with_zero_delay());
  }

  // Create a new list of apps, where there is no app2 (is auto launch now),
  // and present a new app.
  policy::ArcvmKioskAppBasicInfo app3("com.package.name3", "", "", "");
  std::vector<policy::ArcvmKioskAppBasicInfo> new_apps{app1, app3};
  {
    // Observer must be notified once: app list was updated.
    NotificationWaiter waiter(manager());
    SetApps(new_apps, std::string());
    waiter.Wait(1);

    std::vector<const KioskArcvmAppData*> apps = manager()->GetAppsForTesting();
    ASSERT_EQ(2u, apps.size());
    ASSERT_EQ(app1.package_name(), apps[0]->package_name());
    ASSERT_EQ(app3.package_name(), apps[1]->package_name());
    ASSERT_EQ(app1.package_name(), apps[0]->name());
    ASSERT_EQ(app3.package_name(), apps[1]->name());
    // Auto launch app must be reset.
    EXPECT_FALSE(manager()->GetAutoLaunchAccountId().is_valid());
    EXPECT_FALSE(manager()->current_app_was_auto_launched_with_zero_delay());
  }

  // Clean the apps.
  {
    // Observer must be notified once: app list was updated.
    NotificationWaiter waiter(manager());
    CleanApps();
    waiter.Wait(1);

    ASSERT_EQ(0u, manager()->GetAppsForTesting().size());
    EXPECT_FALSE(manager()->GetAutoLaunchAccountId().is_valid());
  }
}

IN_PROC_BROWSER_TEST_F(KioskArcvmAppManagerTest, GetAppByAccountId) {
  const std::string package_name = "com.package.name";

  // Initialize Arc Kiosk apps.
  const policy::ArcvmKioskAppBasicInfo app1(package_name, "", "", "");
  const std::vector<policy::ArcvmKioskAppBasicInfo> init_apps{app1};
  SetApps(init_apps, std::string());

  // Verify the app data searched by account id.
  std::vector<const KioskArcvmAppData*> apps = manager()->GetAppsForTesting();
  ASSERT_EQ(1u, apps.size());
  const KioskArcvmAppData* app = apps[0];
  const KioskArcvmAppData* app_by_account_id =
      manager()->GetAppByAccountId(app->account_id());
  ASSERT_TRUE(app_by_account_id);
  ASSERT_EQ(app_by_account_id, app);
  ASSERT_EQ(app_by_account_id->package_name(), package_name);

  // Verify the null value if the given account id is invalid.
  const AccountId invalid_account_id;
  const KioskArcvmAppData* app_by_invalid_account_id =
      manager()->GetAppByAccountId(invalid_account_id);
  ASSERT_FALSE(app_by_invalid_account_id);
}

IN_PROC_BROWSER_TEST_F(KioskArcvmAppManagerTest, UpdateNameAndIcon) {
  const std::string package_name = "com.package.old";
  const std::string new_name = "new_name";
  const gfx::ImageSkiaRep new_image(gfx::Size(100, 100), 0.0f);
  const gfx::ImageSkia new_icon(new_image);

  // Initialize Arc Kiosk apps.
  const policy::ArcvmKioskAppBasicInfo app1(package_name, "", "", "");
  std::vector<policy::ArcvmKioskAppBasicInfo> init_apps{app1};
  SetApps(init_apps, std::string());

  // Verify the initialized app data.
  std::vector<const KioskArcvmAppData*> apps = manager()->GetAppsForTesting();
  ASSERT_EQ(1u, apps.size());
  const KioskArcvmAppData* app = apps[0];
  ASSERT_EQ(app->name(), package_name);
  ASSERT_TRUE(app->icon().isNull());

  // Update the name and icon of the app, then verify them.
  manager()->UpdateNameAndIcon(app->account_id(), new_name, new_icon);
  ASSERT_EQ(app->name(), new_name);
  ASSERT_FALSE(app->icon().isNull());
  ASSERT_TRUE(app->icon().BackedBySameObjectAs(new_icon));
}

}  // namespace ash

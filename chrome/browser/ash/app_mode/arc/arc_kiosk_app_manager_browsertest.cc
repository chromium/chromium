// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// The purpose of this waiter - wait for being notified some amount of times.
class NotificationWaiter : public KioskAppManagerObserver {
 public:
  // In constructor we provide instance of ArcKioskAppManager and subscribe for
  // notifications from it, and minimum amount of times we expect to get the
  // notification.
  NotificationWaiter(ArcKioskAppManager* manager, int expected_notifications)
      : manager_(manager), expected_notifications_(expected_notifications) {
    manager_->AddObserver(this);
  }
  NotificationWaiter(const NotificationWaiter&) = delete;
  NotificationWaiter& operator=(const NotificationWaiter&) = delete;
  ~NotificationWaiter() override { manager_->RemoveObserver(this); }

  void Wait() {
    if (notification_received_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Returns if the waiter was notified at least expected_notifications_ times.
  bool was_notified() const { return notification_received_; }

 private:
  // KioskAppManagerObserver:
  void OnKioskAppsSettingsChanged() override {
    --expected_notifications_;
    if (expected_notifications_ > 0)
      return;

    notification_received_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  ArcKioskAppManager* manager_;
  bool notification_received_ = false;
  int expected_notifications_;
};

std::string GenerateAccountId(std::string package_name) {
  return package_name + "@ark-kiosk-app";
}

}  // namespace

class ArcKioskAppManagerTest : public InProcessBrowserTest {
 public:
  ArcKioskAppManagerTest() : settings_helper_(false) {}
  ArcKioskAppManagerTest(const ArcKioskAppManagerTest&) = delete;
  ArcKioskAppManagerTest& operator=(const ArcKioskAppManagerTest&) = delete;
  ~ArcKioskAppManagerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(browser()->profile());
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    settings_helper_.RestoreRealDeviceSettingsProvider();
  }

  void SetApps(const std::vector<policy::ArcKioskAppBasicInfo>& apps,
               const std::string& auto_login_account) {
    base::Value::List device_local_accounts;
    for (const policy::ArcKioskAppBasicInfo& app : apps) {
      base::Value::Dict entry;
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyId,
                GenerateAccountId(app.package_name()));
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyType,
                policy::DeviceLocalAccount::TYPE_ARC_KIOSK_APP);
      entry.Set(
          kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
          static_cast<int>(policy::DeviceLocalAccount::EphemeralMode::kUnset));
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage,
                app.package_name());
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyArcKioskClass,
                app.class_name());
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyArcKioskAction,
                app.action());
      entry.Set(kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName,
                app.display_name());
      device_local_accounts.Append(std::move(entry));
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

  void GetApps(std::vector<const ArcKioskAppData*>* apps) const {
    manager()->GetAppsForTesting(apps);
  }

  ArcKioskAppManager* manager() const { return ArcKioskAppManager::Get(); }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;
};

IN_PROC_BROWSER_TEST_F(ArcKioskAppManagerTest, Basic) {
  policy::ArcKioskAppBasicInfo app1("com.package.name1", "", "", "");
  policy::ArcKioskAppBasicInfo app2("com.package.name2", "", "",
                                    "display name");
  std::vector<policy::ArcKioskAppBasicInfo> init_apps{app1, app2};

  // Set initial list of apps.
  {
    // Observer must be notified once: app list was updated.
    NotificationWaiter waiter(manager(), 1);
    SetApps(init_apps, std::string());
    waiter.Wait();
    EXPECT_TRUE(waiter.was_notified());

    std::vector<const ArcKioskAppData*> apps;
    GetApps(&apps);
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
    NotificationWaiter waiter(manager(), 2);
    SetApps(init_apps, GenerateAccountId(app2.package_name()));
    waiter.Wait();
    EXPECT_TRUE(waiter.was_notified());

    EXPECT_TRUE(manager()->GetAutoLaunchAccountId().is_valid());

    std::vector<const ArcKioskAppData*> apps;
    GetApps(&apps);
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
  policy::ArcKioskAppBasicInfo app3("com.package.name3", "", "", "");
  std::vector<policy::ArcKioskAppBasicInfo> new_apps{app1, app3};
  {
    // Observer must be notified once: app list was updated.
    NotificationWaiter waiter(manager(), 1);
    SetApps(new_apps, std::string());
    waiter.Wait();
    EXPECT_TRUE(waiter.was_notified());

    std::vector<const ArcKioskAppData*> apps;
    GetApps(&apps);
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
    NotificationWaiter waiter(manager(), 1);
    CleanApps();
    waiter.Wait();
    EXPECT_TRUE(waiter.was_notified());

    std::vector<const ArcKioskAppData*> apps;
    GetApps(&apps);
    ASSERT_EQ(0u, apps.size());
    EXPECT_FALSE(manager()->GetAutoLaunchAccountId().is_valid());
  }
}

IN_PROC_BROWSER_TEST_F(ArcKioskAppManagerTest, GetAppByAccountId) {
  const std::string package_name = "com.package.name";

  // Initialize Arc Kiosk apps.
  const policy::ArcKioskAppBasicInfo app1(package_name, "", "", "");
  const std::vector<policy::ArcKioskAppBasicInfo> init_apps{app1};
  SetApps(init_apps, std::string());

  // Verify the app data searched by account id.
  std::vector<const ArcKioskAppData*> apps;
  GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  const ArcKioskAppData* app = apps.front();
  const ArcKioskAppData* app_by_account_id =
      manager()->GetAppByAccountId(app->account_id());
  ASSERT_TRUE(app_by_account_id);
  ASSERT_EQ(app_by_account_id, app);
  ASSERT_EQ(app_by_account_id->package_name(), package_name);

  // Verify the null value if the given account id is invalid.
  const AccountId invalid_account_id;
  const ArcKioskAppData* app_by_invalid_account_id =
      manager()->GetAppByAccountId(invalid_account_id);
  ASSERT_FALSE(app_by_invalid_account_id);
}

IN_PROC_BROWSER_TEST_F(ArcKioskAppManagerTest, UpdateNameAndIcon) {
  const std::string package_name = "com.package.old";
  const std::string new_name = "new_name";
  const gfx::ImageSkiaRep new_image(gfx::Size(100, 100), 0.0f);
  const gfx::ImageSkia new_icon(new_image);

  // Initialize Arc Kiosk apps.
  const policy::ArcKioskAppBasicInfo app1(package_name, "", "", "");
  std::vector<policy::ArcKioskAppBasicInfo> init_apps{app1};
  SetApps(init_apps, std::string());

  // Verify the initialized app data.
  std::vector<const ArcKioskAppData*> apps;
  GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  const ArcKioskAppData* app = apps.front();
  ASSERT_EQ(app->name(), package_name);
  ASSERT_TRUE(app->icon().isNull());

  // Update the name and icon of the app, then verify them.
  manager()->UpdateNameAndIcon(app->account_id(), new_name, new_icon);
  ASSERT_EQ(app->name(), new_name);
  ASSERT_FALSE(app->icon().isNull());
  ASSERT_TRUE(app->icon().BackedBySameObjectAs(new_icon));
}

}  // namespace ash

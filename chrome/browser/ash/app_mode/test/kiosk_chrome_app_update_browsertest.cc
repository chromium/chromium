// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using kiosk::test::CachedChromeAppVersion;
using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::LaunchAppManually;
using kiosk::test::LocalFsChromeAppV1;
using kiosk::test::LocalFsChromeAppV2;
using kiosk::test::OfflineEnabledChromeAppV1;
using kiosk::test::OfflineEnabledChromeAppV2;
using kiosk::test::OfflineEnabledChromeAppV2WithPermissionChange;
using kiosk::test::TheKioskChromeApp;
using kiosk::test::WaitKioskLaunched;

void ServeAppOnFakeCws(FakeCWS& fake_cws,
                       const KioskMixin::CwsChromeAppOption& app) {
  fake_cws.SetUpdateCrx(app.app_id, app.crx_filename, app.crx_version);
}

// Triggers an update on the external cache and verifies it updates `app_id` to
// `version`.
void TriggerCacheUpdateAndWait(std::string app_id, std::string crx_version) {
  auto& manager = CHECK_DEREF(KioskChromeAppManager::Get());
  TestAppDataLoadWaiter waiter(&manager, app_id, crx_version);
  manager.UpdateExternalCache();
  waiter.Wait();
}

}  // namespace

// Verifies Chrome app update behavior in Kiosk.
class KioskChromeAppUpdateTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskChromeAppUpdateTest() = default;
  KioskChromeAppUpdateTest(const KioskChromeAppUpdateTest&) = delete;
  KioskChromeAppUpdateTest& operator=(const KioskChromeAppUpdateTest&) = delete;
  ~KioskChromeAppUpdateTest() override = default;

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/{},
          KioskMixin::AutoLaunchAccount{OfflineEnabledChromeAppV1().account_id},
          {OfflineEnabledChromeAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(KioskChromeAppUpdateTest,
                       PRE_UpdatesAppOfflineFromCache) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());

  // External cache has app version 1.
  std::string app_id = TheKioskChromeApp().id().app_id.value();
  auto app_v1 = OfflineEnabledChromeAppV1();
  ASSERT_EQ(app_id, app_v1.app_id);
  ASSERT_EQ(CachedChromeAppVersion(TheKioskChromeApp()), app_v1.crx_version);

  // The installed app is also version 1.
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            app_v1.crx_version);

  // Update app in `FakeCWS` to version 2.
  auto app_v2 = OfflineEnabledChromeAppV2();
  ASSERT_EQ(app_id, app_v2.app_id);
  ServeAppOnFakeCws(kiosk_.fake_cws(), app_v2);

  // Update the external cache to version 2.
  TriggerCacheUpdateAndWait(app_id, app_v2.crx_version);
  ASSERT_EQ(CachedChromeAppVersion(TheKioskChromeApp()), app_v2.crx_version);

  // Verify the installed app is still version 1.
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            app_v1.crx_version);
}

IN_PROC_BROWSER_TEST_F(KioskChromeAppUpdateTest, UpdatesAppOfflineFromCache) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV2().crx_version);
}

IN_PROC_BROWSER_TEST_F(KioskChromeAppUpdateTest,
                       PRE_LaunchesAppWhenItHasNoUpdate) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskChromeAppUpdateTest, LaunchesAppWhenItHasNoUpdate) {
  kiosk_.fake_cws().SetNoUpdate(OfflineEnabledChromeAppV1().app_id);
  network_state_.SimulateOnline();

  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
}

// Verifies Chrome app update behavior in Kiosk. Similar to the previous
// `KioskChromeAppUpdateTest` class, but launches apps manually to better
// control launch timings.
class KioskManualLaunchChromeAppUpdateTest
    : public MixinBasedInProcessBrowserTest {
 public:
  KioskManualLaunchChromeAppUpdateTest() = default;
  KioskManualLaunchChromeAppUpdateTest(
      const KioskManualLaunchChromeAppUpdateTest&) = delete;
  KioskManualLaunchChromeAppUpdateTest& operator=(
      const KioskManualLaunchChromeAppUpdateTest&) = delete;
  ~KioskManualLaunchChromeAppUpdateTest() override = default;

  bool LaunchTheAppAndWaitSession() {
    return LaunchAppManually(TheKioskChromeApp()) && WaitKioskLaunched();
  }

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/KioskMixin::Config{
                        /*name=*/{},
                        /*auto_launch_account_id=*/{},
                        {OfflineEnabledChromeAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       PRE_InstallsAppOfflineFromCache) {
  network_state_.SimulateOnline();
  auto app_v1 = OfflineEnabledChromeAppV1();
  TriggerCacheUpdateAndWait(app_v1.app_id, app_v1.crx_version);
}

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       InstallsAppOfflineFromCache) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchTheAppAndWaitSession());
}

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       PRE_UpdatesAppDuringLaunch) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchTheAppAndWaitSession());
}

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       UpdatesAppDuringLaunch) {
  auto app_v2 = OfflineEnabledChromeAppV2();
  ServeAppOnFakeCws(kiosk_.fake_cws(), app_v2);

  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchTheAppAndWaitSession());

  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            app_v2.crx_version);
}

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       PRE_UpdatesAppWithPermissionChange) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchTheAppAndWaitSession());
}

IN_PROC_BROWSER_TEST_F(KioskManualLaunchChromeAppUpdateTest,
                       UpdatesAppWithPermissionChange) {
  network_state_.SimulateOffline();
  auto app_v2 = OfflineEnabledChromeAppV2WithPermissionChange();
  ServeAppOnFakeCws(kiosk_.fake_cws(), app_v2);

  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchTheAppAndWaitSession());

  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            app_v2.crx_version);
}

// Verifies Chrome app data is maintained across updates in Kiosk.
class KioskChromeAppDataUpdateTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskChromeAppDataUpdateTest() = default;
  KioskChromeAppDataUpdateTest(const KioskChromeAppDataUpdateTest&) = delete;
  KioskChromeAppDataUpdateTest& operator=(const KioskChromeAppDataUpdateTest&) =
      delete;
  ~KioskChromeAppDataUpdateTest() override = default;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/"ChromeApp",
          KioskMixin::AutoLaunchAccount{LocalFsChromeAppV1().account_id},
          {LocalFsChromeAppV1()}}};

 private:
  // Corresponds to the Chrome app in:
  //   //chrome/test/data/chromeos/app_mode/apps_and_extensions/local_fs/
  //
  // Version 1 of this app writes some data to the file system, while version 2
  // reads from it.
  static constexpr char kLocalFsChromeAppId[] =
      "abbjjkefakmllanciinhgjgjamdmlbdg";
};

IN_PROC_BROWSER_TEST_F(KioskChromeAppDataUpdateTest,
                       PRE_PreservesLocalDataAcrossUpdates) {
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(KioskChromeAppDataUpdateTest,
                       PreservesLocalDataAcrossUpdates) {
  ServeAppOnFakeCws(kiosk_.fake_cws(), LocalFsChromeAppV2());

  extensions::ResultCatcher catcher;
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace ash

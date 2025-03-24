// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::AppWithSecondaryAppV1;
using kiosk::test::CachedChromeAppVersion;
using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::LaunchAppManually;
using kiosk::test::MinimumChromeVersionAppV1;
using kiosk::test::MinimumChromeVersionAppV2WithMinimumVersion100;
using kiosk::test::MinimumChromeVersionAppV3WithMinimumVersion999;
using kiosk::test::TheKioskChromeApp;
using kiosk::test::WaitKioskLaunched;

namespace {

bool IsChromeAppCached(const KioskApp& app) {
  auto& manager = CHECK_DEREF(KioskChromeAppManager::Get());
  return manager.GetCachedCrx(app.id().app_id.value()).has_value();
}

void ServeAppOnFakeCws(FakeCWS& fake_cws, KioskMixin::CwsChromeAppOption app) {
  fake_cws.SetUpdateCrx(app.app_id, app.crx_filename, app.crx_version);
}

}  // namespace

// Verifies Chrome apps with the `minimum_chrome_version` manifest field work
// correctly in Kiosk.
class MinimumChromeVersionTest : public MixinBasedInProcessBrowserTest {
 public:
  MinimumChromeVersionTest() = default;
  MinimumChromeVersionTest(const MinimumChromeVersionTest&) = delete;
  MinimumChromeVersionTest& operator=(const MinimumChromeVersionTest&) = delete;
  ~MinimumChromeVersionTest() override = default;

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/
                    KioskMixin::Config{/*name=*/{},
                                       /*auto_launch_account_id=*/{},
                                       {MinimumChromeVersionAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(MinimumChromeVersionTest,
                       PRE_UpdatesWhenChromeVersionIsNew) {
  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  auto version1 = MinimumChromeVersionAppV1().crx_version;
  EXPECT_EQ(version1, CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(version1,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));
}

IN_PROC_BROWSER_TEST_F(MinimumChromeVersionTest,
                       UpdatesWhenChromeVersionIsNew) {
  auto app_v2 = MinimumChromeVersionAppV2WithMinimumVersion100();
  ServeAppOnFakeCws(kiosk_.fake_cws(), app_v2);

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(app_v2.crx_version, CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(app_v2.crx_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));
}

IN_PROC_BROWSER_TEST_F(MinimumChromeVersionTest,
                       PRE_DoesNotUpdateWhenChromeVersionIsOld) {
  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  auto version1 = MinimumChromeVersionAppV1().crx_version;
  EXPECT_EQ(version1, CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(version1,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));
}

IN_PROC_BROWSER_TEST_F(MinimumChromeVersionTest,
                       DoesNotUpdateWhenChromeVersionIsOld) {
  ServeAppOnFakeCws(kiosk_.fake_cws(),
                    MinimumChromeVersionAppV3WithMinimumVersion999());

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  // The app is removed from the cache once the update fails.
  EXPECT_FALSE(IsChromeAppCached(TheKioskChromeApp()));
  EXPECT_EQ(MinimumChromeVersionAppV1().crx_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));
}

// Verifies a secondary Chrome app with the `minimum_chrome_version` manifest
// field works correctly in Kiosk.
class SecondaryAppMinimumChromeVersionTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SecondaryAppMinimumChromeVersionTest() = default;
  SecondaryAppMinimumChromeVersionTest(
      const SecondaryAppMinimumChromeVersionTest&) = delete;
  SecondaryAppMinimumChromeVersionTest& operator=(
      const SecondaryAppMinimumChromeVersionTest&) = delete;
  ~SecondaryAppMinimumChromeVersionTest() override = default;

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/
                    KioskMixin::Config{/*name=*/{},
                                       /*auto_launch_account_id=*/{},
                                       {AppWithSecondaryAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(SecondaryAppMinimumChromeVersionTest,
                       PRE_UpdatesWhenChromeVersionIsNew) {
  auto primary_app_version = AppWithSecondaryAppV1().crx_version;
  auto secondary_app_v1 = MinimumChromeVersionAppV1();
  ServeAppOnFakeCws(kiosk_.fake_cws(), secondary_app_v1);

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(primary_app_version, CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(primary_app_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));

  EXPECT_EQ(
      secondary_app_v1.crx_version,
      InstalledChromeAppVersion(CurrentProfile(), secondary_app_v1.app_id));
}

IN_PROC_BROWSER_TEST_F(SecondaryAppMinimumChromeVersionTest,
                       UpdatesWhenChromeVersionIsNew) {
  auto secondary_app_v2 = MinimumChromeVersionAppV2WithMinimumVersion100();
  ServeAppOnFakeCws(kiosk_.fake_cws(), secondary_app_v2);

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(
      secondary_app_v2.crx_version,
      InstalledChromeAppVersion(CurrentProfile(), secondary_app_v2.app_id));
}

IN_PROC_BROWSER_TEST_F(SecondaryAppMinimumChromeVersionTest,
                       PRE_DoesNotUpdateWhenChromeVersionIsOld) {
  auto primary_app_version = AppWithSecondaryAppV1().crx_version;
  auto secondary_app_v1 = MinimumChromeVersionAppV1();
  ServeAppOnFakeCws(kiosk_.fake_cws(), secondary_app_v1);

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(primary_app_version, CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(primary_app_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));

  EXPECT_EQ(
      secondary_app_v1.crx_version,
      InstalledChromeAppVersion(CurrentProfile(), secondary_app_v1.app_id));
}

IN_PROC_BROWSER_TEST_F(SecondaryAppMinimumChromeVersionTest,
                       DoesNotUpdateWhenChromeVersionIsOld) {
  ServeAppOnFakeCws(kiosk_.fake_cws(),
                    MinimumChromeVersionAppV3WithMinimumVersion999());

  ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  auto secondary_app_v1 = MinimumChromeVersionAppV1();
  EXPECT_EQ(
      secondary_app_v1.crx_version,
      InstalledChromeAppVersion(CurrentProfile(), secondary_app_v1.app_id));
}

}  // namespace ash

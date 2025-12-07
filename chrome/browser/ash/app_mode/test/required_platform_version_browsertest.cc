// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string_view>

#include "base/check_deref.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CachedChromeAppVersion;
using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::OfflineEnabledChromeAppV1;
using kiosk::test::OfflineEnabledChromeAppV2RequiresVersion1234;
using kiosk::test::TheKioskChromeApp;
using kiosk::test::WaitKioskLaunched;

namespace {

void ServeAppOnFakeCws(FakeCWS& fake_cws,
                       const KioskMixin::CwsChromeAppOption& app) {
  fake_cws.SetUpdateCrx(app.app_id, app.crx_filename, app.crx_version);
}

bool HasPendingUpdate(Profile& profile, const KioskApp& app) {
  auto& service = CHECK_DEREF(
      extensions::ExtensionSystem::Get(&profile)->extension_service());
  auto* extension_update =
      service.GetPendingExtensionUpdate(app.id().app_id.value());
  return extension_update != nullptr;
}

}  // namespace

// Verifies the Chrome app manifest field `required_platform_version` used in
// Kiosk.
class RequiredPlatformVersionTest : public MixinBasedInProcessBrowserTest {
 public:
  RequiredPlatformVersionTest() = default;
  RequiredPlatformVersionTest(const RequiredPlatformVersionTest&) = delete;
  RequiredPlatformVersionTest& operator=(const RequiredPlatformVersionTest&) =
      delete;
  ~RequiredPlatformVersionTest() override = default;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/"ChromeApp",
          KioskMixin::AutoLaunchAccount{OfflineEnabledChromeAppV1().account_id},
          {OfflineEnabledChromeAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(RequiredPlatformVersionTest,
                       InstallsAppOnNonCompliantRequiredPlatformVersion) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1233.0.0", base::Time::Now());

  ServeAppOnFakeCws(kiosk_.fake_cws(),
                    OfflineEnabledChromeAppV2RequiresVersion1234());

  ASSERT_TRUE(WaitKioskLaunched());

  // Since there is no "good" version of the app pre-installed, Kiosk will
  // install it the first time even if the OS version does not meet
  // `required_platform_version`. This way Kiosk at least tries to launch the
  // app in a best effort basis, instead of failing launch altogether.
  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV2RequiresVersion1234().crx_version);
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV2RequiresVersion1234().crx_version);
}

IN_PROC_BROWSER_TEST_F(RequiredPlatformVersionTest,
                       PRE_DoesNotUpdateToNonCompliantRequiredPlatformVersion) {
  ServeAppOnFakeCws(kiosk_.fake_cws(), OfflineEnabledChromeAppV1());
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
}

IN_PROC_BROWSER_TEST_F(RequiredPlatformVersionTest,
                       DoesNotUpdateToNonCompliantRequiredPlatformVersion) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1233.0.0", base::Time::Now());

  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);

  ServeAppOnFakeCws(kiosk_.fake_cws(),
                    OfflineEnabledChromeAppV2RequiresVersion1234());

  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV2RequiresVersion1234().crx_version);
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
  EXPECT_TRUE(HasPendingUpdate(CurrentProfile(), TheKioskChromeApp()));
}

IN_PROC_BROWSER_TEST_F(RequiredPlatformVersionTest,
                       PRE_UpdatesToCompliantRequiredPlatformVersion) {
  ServeAppOnFakeCws(kiosk_.fake_cws(), OfflineEnabledChromeAppV1());
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);
}

IN_PROC_BROWSER_TEST_F(RequiredPlatformVersionTest,
                       UpdatesToCompliantRequiredPlatformVersion) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1234.0.0", base::Time::Now());

  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV1().crx_version);

  ServeAppOnFakeCws(kiosk_.fake_cws(),
                    OfflineEnabledChromeAppV2RequiresVersion1234());

  ASSERT_TRUE(WaitKioskLaunched());
  EXPECT_EQ(CachedChromeAppVersion(TheKioskChromeApp()),
            OfflineEnabledChromeAppV2RequiresVersion1234().crx_version);
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()),
            OfflineEnabledChromeAppV2RequiresVersion1234().crx_version);
}

}  // namespace ash

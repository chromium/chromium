// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_accelerators.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace apps {

using ash::KioskAppLaunchError;
using ash::KioskMixin;
using ash::kiosk::test::LaunchAppManually;
using ash::kiosk::test::OfflineEnabledChromeAppV1;
using ash::kiosk::test::WaitKioskLaunched;

enum AllowChromeAppsFlag {
  kDefault,  // by default the flag is disabled.
  kEnabled,
};

template <AllowChromeAppsFlag flag>
class ChromeAppDeprecationKioskSessionBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ChromeAppDeprecationKioskSessionBrowserTest() {
    switch (flag) {
      case kDefault:
        scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
        break;
      case kEnabled:
        scoped_feature_list_.InitAndEnableFeature(
            chrome_app_deprecation::kAllowChromeAppsInKioskSessions);
        break;
    }
  }

  ChromeAppDeprecationKioskSessionBrowserTest(
      const ChromeAppDeprecationKioskSessionBrowserTest&) = delete;
  ChromeAppDeprecationKioskSessionBrowserTest& operator=(
      const ChromeAppDeprecationKioskSessionBrowserTest&) = delete;
  ~ChromeAppDeprecationKioskSessionBrowserTest() override = default;

  void WaitForChromeAppDeprecatedLaunchError() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return KioskAppLaunchError::Get() ==
             KioskAppLaunchError::Error::kChromeAppDeprecated;
    }));
  }

  bool PressBailoutAccelerator() {
    return ash::LoginDisplayHost::default_host()->HandleAccelerator(
        ash::LoginAcceleratorAction::kAppLaunchBailout);
  }

  void VerifyUniqueKioskLaunchErrorMetric(KioskAppLaunchError::Error error) {
    histogram_.ExpectUniqueSample(ash::kKioskLaunchErrorHistogram, error,
                                  /*expected_bucket_count=*/1);
  }

 private:
  base::HistogramTester histogram_;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/{},
          KioskMixin::AutoLaunchAccount{OfflineEnabledChromeAppV1().account_id},
          {OfflineEnabledChromeAppV1()}}};
  base::test::ScopedFeatureList scoped_feature_list_;
};

using ChromeAppDeprecationKioskDefaultFlagTest =
    ChromeAppDeprecationKioskSessionBrowserTest<AllowChromeAppsFlag::kDefault>;

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       StuckOnSplashScreen) {
  WaitForChromeAppDeprecatedLaunchError();

  base::RunLoop().RunUntilIdle();

  // Check kiosk is not started and user is stuck on the splash screen.
  EXPECT_EQ(ash::KioskController::Get().GetKioskSystemSession(), nullptr);
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());
}

// Bailout accelerator should work on the splash screen with the Chrome App
// deprecated message.
IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       UserCancelled) {
  WaitForChromeAppDeprecatedLaunchError();
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());

  PressBailoutAccelerator();

  RunUntilBrowserProcessQuits();
  EXPECT_EQ(KioskAppLaunchError::Get(),
            KioskAppLaunchError::Error::kUserCancel);
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       AllowlistedAppCanBeLaunched) {
  chrome_app_deprecation::AddAppToAllowlistForTesting(
      OfflineEnabledChromeAppV1().app_id);

  ASSERT_FALSE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));

  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       PRE_CheckMetric) {
  WaitForChromeAppDeprecatedLaunchError();
}

// Metrics are recorded on the next browser start.
IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest, CheckMetric) {
  VerifyUniqueKioskLaunchErrorMetric(
      KioskAppLaunchError::Error::kChromeAppDeprecated);
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       PRE_CheckAutoLaunchWorks) {
  WaitForChromeAppDeprecatedLaunchError();
}

// When Chrome App is deprecated, the auto-launch after the browser restart
// should proceed and stuck on the same splash screen.
IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       CheckAutoLaunchWorks) {
  WaitForChromeAppDeprecatedLaunchError();

  base::RunLoop().RunUntilIdle();

  // Check kiosk is not started and user is stuck on the splash screen.
  EXPECT_EQ(ash::KioskController::Get().GetKioskSystemSession(), nullptr);
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       PRE_NoAutoLaunchAfterUserCancelled) {
  WaitForChromeAppDeprecatedLaunchError();
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());

  PressBailoutAccelerator();

  RunUntilBrowserProcessQuits();
  EXPECT_EQ(KioskAppLaunchError::Get(),
            KioskAppLaunchError::Error::kUserCancel);
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

// Keep the existing behavior -- if user cancelled kiosk launch, app should not
// be auto-launched again.
IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskDefaultFlagTest,
                       NoAutoLaunchAfterUserCancelled) {
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

using ChromeAppDeprecationKioskEnabledFeatureFlagTest =
    ChromeAppDeprecationKioskSessionBrowserTest<AllowChromeAppsFlag::kEnabled>;

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationKioskEnabledFeatureFlagTest,
                       LaunchChromeApp) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));
  ASSERT_TRUE(WaitKioskLaunched());
}

}  // namespace apps

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"

#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/message_center/message_center.h"

using ash::KioskMixin;
using ash::kiosk::test::LaunchAppManually;
using ash::kiosk::test::OfflineEnabledChromeAppV1;
using ash::kiosk::test::WaitKioskLaunched;

namespace apps {

using extensions::Extension;

class ChromeAppDeprecationUserInstalledAppsBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  const Extension* LoadPlatformApp() {
    const base::FilePath& root_path = test_data_dir_;
    base::FilePath extension_path =
        root_path.AppendASCII("platform_apps/launch");

    return LoadExtension(extension_path);
  }

  void RunPlatformApp(const Extension* extension) {
    AppLaunchParams params(
        extension->id(), LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, LaunchSource::kFromTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();

    AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->LaunchAppWithParams(std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       NotAllowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  RunPlatformApp(app);
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count + 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       Allowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  chrome_app_deprecation::AddAppToAllowlistForTesting(app->id());

  RunPlatformApp(app);
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count);
}

class ChromeAppDeprecationKioskSessionBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ChromeAppDeprecationKioskSessionBrowserTest() = default;
  ChromeAppDeprecationKioskSessionBrowserTest(
      const ChromeAppDeprecationKioskSessionBrowserTest&) = delete;
  ChromeAppDeprecationKioskSessionBrowserTest& operator=(
      const ChromeAppDeprecationKioskSessionBrowserTest&) = delete;
  ~ChromeAppDeprecationKioskSessionBrowserTest() override = default;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/"ChromeApp",
          KioskMixin::AutoLaunchAccount{OfflineEnabledChromeAppV1().account_id},
          {OfflineEnabledChromeAppV1()}}};

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class KioskDefaultFeatureFlag
    : public ChromeAppDeprecationKioskSessionBrowserTest {
 public:
  KioskDefaultFeatureFlag() {
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }
  KioskDefaultFeatureFlag(const KioskDefaultFeatureFlag&) = delete;
  KioskDefaultFeatureFlag& operator=(const KioskDefaultFeatureFlag&) = delete;
  ~KioskDefaultFeatureFlag() override = default;
};

IN_PROC_BROWSER_TEST_F(KioskDefaultFeatureFlag, FeatureFlag) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));

  RunUntilBrowserProcessQuits();
  EXPECT_EQ(ash::KioskAppLaunchError::Error::kChromeAppDeprecated,
            ash::KioskAppLaunchError::Get());
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

class KioskEnabledFeatureFlag
    : public ChromeAppDeprecationKioskSessionBrowserTest {
 public:
  KioskEnabledFeatureFlag() {
    scoped_feature_list_.InitAndEnableFeature(
        chrome_app_deprecation::kAllowChromeAppsInKioskSessions);
  }
  KioskEnabledFeatureFlag(const KioskEnabledFeatureFlag&) = delete;
  KioskEnabledFeatureFlag& operator=(const KioskEnabledFeatureFlag&) = delete;
  ~KioskEnabledFeatureFlag() override = default;
};

IN_PROC_BROWSER_TEST_F(KioskEnabledFeatureFlag, FeatureFlag) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));
  ASSERT_TRUE(WaitKioskLaunched());
}

class KioskDisabledFeatureFlag
    : public ChromeAppDeprecationKioskSessionBrowserTest {
 public:
  KioskDisabledFeatureFlag() {
    scoped_feature_list_.InitAndDisableFeature(
        chrome_app_deprecation::kAllowChromeAppsInKioskSessions);
  }
  KioskDisabledFeatureFlag(const KioskDisabledFeatureFlag&) = delete;
  KioskDisabledFeatureFlag& operator=(const KioskDisabledFeatureFlag&) = delete;
  ~KioskDisabledFeatureFlag() override = default;
};

IN_PROC_BROWSER_TEST_F(KioskDisabledFeatureFlag, FeatureFlag) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));

  RunUntilBrowserProcessQuits();
  EXPECT_EQ(ash::KioskAppLaunchError::Error::kChromeAppDeprecated,
            ash::KioskAppLaunchError::Get());
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

class KioskDisabledFeatureFlagWithAllowlistedApp
    : public ChromeAppDeprecationKioskSessionBrowserTest {
 public:
  KioskDisabledFeatureFlagWithAllowlistedApp() {
    scoped_feature_list_.InitAndDisableFeature(
        chrome_app_deprecation::kAllowChromeAppsInKioskSessions);
    chrome_app_deprecation::AddAppToAllowlistForTesting(
        OfflineEnabledChromeAppV1().app_id);
  }
  KioskDisabledFeatureFlagWithAllowlistedApp(
      const KioskDisabledFeatureFlagWithAllowlistedApp&) = delete;
  KioskDisabledFeatureFlagWithAllowlistedApp& operator=(
      const KioskDisabledFeatureFlagWithAllowlistedApp&) = delete;
  ~KioskDisabledFeatureFlagWithAllowlistedApp() override = default;
};

IN_PROC_BROWSER_TEST_F(KioskDisabledFeatureFlagWithAllowlistedApp,
                       AllowlistedApp) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      chrome_app_deprecation::kAllowChromeAppsInKioskSessions));

  ASSERT_TRUE(WaitKioskLaunched());
}
}  // namespace apps

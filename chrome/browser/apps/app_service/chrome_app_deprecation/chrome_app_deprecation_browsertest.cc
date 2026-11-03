// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"

#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/message_center/message_center.h"

using extensions::Extension;

namespace apps::chrome_app_deprecation {

class ChromeAppDeprecationUserInstalledAppsBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));
  }

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
        ui::mojom::WindowOpenDisposition::kNewWindow, LaunchSource::kFromTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();

    AppServiceProxyFactory::GetForProfile(browser()->GetProfile())
        ->LaunchAppWithParams(std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       NotAllowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  ScopedSkipSystemDialogForTesting skip_system_dialog;

  RunPlatformApp(app);

  // No notifications are displayed when the launch is blocked.
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count);
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       Allowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  apps::chrome_app_deprecation::ScopedAddAppToAllowlistForTesting allowlist(
      app->id());

  RunPlatformApp(app);

  // No notifications are displayed when the launch is allowed.
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count);
}

class ChromeAppDeprecationAdminInstalledAppsBrowserTest
    : public ChromeAppDeprecationUserInstalledAppsBrowserTest {
 public:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowAdminInstalledChromeApps));
  }

 protected:
  const Extension* LoadAdminInstalledPlatformApp() {
    const base::FilePath& root_path = test_data_dir_;
    base::FilePath extension_path =
        root_path.AppendASCII("platform_apps/launch");

    return InstallExtension(
        extension_path, /*expected_change=*/1,
        extensions::mojom::ManifestLocation::kExternalPolicy);
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationAdminInstalledAppsBrowserTest,
                       DefaultFeatureFlagAllowsLaunch) {
  const Extension* app = LoadAdminInstalledPlatformApp();
  ASSERT_TRUE(app);

  ExtensionTestMessageListener launched_listener("Launched");
  RunPlatformApp(app);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  histogram_tester_.ExpectUniqueSample(
      "Apps.AppLaunch.ChromeAppsDeprecationCheck",
      /*DeprecationCheckOutcome::kManagedAllowedByFlag*/ 7, 1);
}

class ChromeAppDeprecationAdminInstalledAppsDisabledFlagBrowserTest
    : public ChromeAppDeprecationAdminInstalledAppsBrowserTest {
 public:
  ChromeAppDeprecationAdminInstalledAppsDisabledFlagBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kAllowAdminInstalledChromeApps);
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowAdminInstalledChromeApps));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ChromeAppDeprecationAdminInstalledAppsDisabledFlagBrowserTest,
    NotAllowlistedBlocked) {
  const Extension* app = LoadAdminInstalledPlatformApp();
  ASSERT_TRUE(app);

  ScopedSkipSystemDialogForTesting skip_system_dialog;
  RunPlatformApp(app);

  histogram_tester_.ExpectUniqueSample(
      "Apps.AppLaunch.ChromeAppsDeprecationCheck",
      /*DeprecationCheckOutcome::kManagedBlocked*/ 10, 1);
}

IN_PROC_BROWSER_TEST_F(
    ChromeAppDeprecationAdminInstalledAppsDisabledFlagBrowserTest,
    AllowlistedAllowsLaunch) {
  const Extension* app = LoadAdminInstalledPlatformApp();
  ASSERT_TRUE(app);

  ScopedAddAppToAllowlistForTesting allowlist(app->id());

  ExtensionTestMessageListener launched_listener("Launched");
  RunPlatformApp(app);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  histogram_tester_.ExpectUniqueSample(
      "Apps.AppLaunch.ChromeAppsDeprecationCheck",
      /*DeprecationCheckOutcome::kManagedAllowedByAllowlist*/ 8, 1);
}

IN_PROC_BROWSER_TEST_F(
    ChromeAppDeprecationAdminInstalledAppsDisabledFlagBrowserTest,
    PolicyOverrideAllowsLaunch) {
  const Extension* app = LoadAdminInstalledPlatformApp();
  ASSERT_TRUE(app);

  browser()->GetProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kAdminInstalledChromeAppsForceAllowed, true);

  ExtensionTestMessageListener launched_listener("Launched");
  RunPlatformApp(app);
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

  histogram_tester_.ExpectUniqueSample(
      "Apps.AppLaunch.ChromeAppsDeprecationCheck",
      /*DeprecationCheckOutcome::kManagedAllowedByAdminPolicy*/ 9, 1);
}

}  // namespace apps::chrome_app_deprecation

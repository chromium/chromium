// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/for_which_extension_type.h"
#include "chrome/browser/lacros/lacros_extension_apps_controller.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "content/public/test/browser_test.h"
#include "extension_keeplist_chromeos.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace extensions {

class LacrosExtensionKeeplistTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ash_keeplist_browser_init_params_supported_ =
        !chromeos::BrowserParamsProxy::Get()->ExtensionKeepList().is_null();
  }

 protected:
  bool AshKeeplistFromBrowserInitParamsSupported() {
    return ash_keeplist_browser_init_params_supported_;
  }

 private:
  bool ash_keeplist_browser_init_params_supported_ = false;
};

// Test the Ash extension keeplist data in Lacros against Ash versions that
// support passing Ash extension keep list to Lacros with
// crosapi::mojom::BrowserInitParams.
IN_PROC_BROWSER_TEST_F(LacrosExtensionKeeplistTest,
                       AshKeeplistFromBrowserInitParamsSupported) {
  // This test does not apply to unsupported ash version.
  if (!AshKeeplistFromBrowserInitParamsSupported()) {
    GTEST_SKIP();
  }

  // For Ash running in the version that supports passing Ash extension keep
  // list to Lacros with crosapi::mojom::BrowserInitParams, just do some minimum
  // sanity check to make sure the extension list passed from Ash is not empty.
  // We have a more sophiscaited test in extension_keeplist_ash_browsertest.cc
  // to verify the keep lists are idnetical in Ash and Lacros for such case.
  EXPECT_FALSE(extensions::GetExtensionsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(
      extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(extensions::GetExtensionsRunInOSOnly().empty());
  EXPECT_FALSE(extensions::GetExtensionAppsRunInOSOnly().empty());
}

// Test the Ash extension keeplist data in Lacros against older Ash versions
// that do NOT support passing Ash extension keep list to Lacros with
// crosapi::mojom::BrowserInitParams.
IN_PROC_BROWSER_TEST_F(LacrosExtensionKeeplistTest,
                       AshKeeplistFromBrowserInitParamsNotSupported) {
  // This test only applies to older ash version which does not support
  // passing Ash extension keeplist data via crosapi::mojom::BrowserInitParams.
  if (AshKeeplistFromBrowserInitParamsSupported()) {
    GTEST_SKIP();
  }

  // Verify that Lacros uses the static compiled ash extension keep list.
  // This tests the backward compatibility support of ash extension keeplist.
  ASSERT_EQ(extensions::GetExtensionsRunInOSAndStandaloneBrowser().size(),
            ExtensionsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().size(),
            ExtensionAppsRunInOSAndStandaloneBrowserAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionsRunInOSOnly().size(),
            ExtensionsRunInOSOnlyAllowlistSizeForTest());
  ASSERT_EQ(extensions::GetExtensionAppsRunInOSOnly().size(),
            ExtensionAppsRunInOSOnlyAllowlistSizeForTest());
}

class ExtensionAppsAppServiceBlocklistTest
    : public extensions::ExtensionBrowserTest {
 public:
  void InstallFakeGnubbydApp() {
    DCHECK(gnubbyd_app_id_.empty());
    const extensions::Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("ash_extension_keeplist/fake_gnubbyd_app"));
    gnubbyd_app_id_ = extension->id();
    EXPECT_EQ(gnubbyd_app_id_, extension_misc::kGnubbyAppId);
  }

  void InstallFakeGCSEExtension() {
    DCHECK(!gcse_extension_);
    gcse_extension_ = LoadExtension(test_data_dir_.AppendASCII(
        "ash_extension_keeplist/fake_GCSE_extension"));
    EXPECT_EQ(gcse_extension_->id(), extension_misc::kGCSEExtensionId);
  }

  const std::string& gnubbyd_app_id() const { return gnubbyd_app_id_; }
  const extensions::Extension* gcse_extension() { return gcse_extension_; }

 private:
  // extensions::ExtensionBrowserTest:
  void TearDownOnMainThread() override {
    CloseAllAppWindows();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void CloseAllAppWindows() {
    for (extensions::AppWindow* app_window :
         extensions::AppWindowRegistry::Get(profile())->app_windows()) {
      app_window->GetBaseWindow()->Close();
    }

    // Wait for item to stop existing in shelf.
    if (!gnubbyd_app_id_.empty()) {
      ASSERT_TRUE(browser_test_util::WaitForShelfItem(gnubbyd_app_id_,
                                                      /*exists=*/false));
    }
  }

  std::string gnubbyd_app_id_;
  raw_ptr<const extensions::Extension> gcse_extension_ = nullptr;
};

// This tests publishing and launching gnubbyd app (running in both ash and
// lacros) with app service. It installs a fake gnubbyd app to simulate the test
// case.
// TODO(crbug.com/1409199): Remove the fake gnubbyd app, and configure ash with
// a testing app to exercise the test case.
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       GnubbydAppLaunchInAppList) {
  if (!extensions::IsAppServiceBlocklistCrosapiSupported()) {
    GTEST_SKIP() << "Unsupported ash version";
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  InstallFakeGnubbydApp();

  EXPECT_TRUE(extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(
      gnubbyd_app_id()));
  EXPECT_FALSE(
      extensions::ExtensionAppBlockListedForAppServiceInStandaloneBrowser(
          gnubbyd_app_id()));

  // Gnubbyd item should not exist in the shelf before the app is launched.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(gnubbyd_app_id(), /*exists=*/false));

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // The fake gnubbyd app should have been published in app service by lacros,
  // and can be launched from app list.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->LaunchAppFromAppList(gnubbyd_app_id());

  // Wait for item to exist in shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(gnubbyd_app_id(), /*exists=*/true));
}

// This tests the backward compatibility for gnubbyd app with older ash which
// does not supports app list block list. It installs a fake gnubbyd app to
// simulate the test case.
// TODO(crbug.com/1409199): Remove the fake gnubbyd app, and configure ash
// with a testing app to exercise the test case.
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       GnubbydAppNotLaunchInAppList) {
  if (extensions::IsAppServiceBlocklistCrosapiSupported()) {
    GTEST_SKIP()
        << "This test should not run with the new ash supporting app service "
           "block list";
  }

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  InstallFakeGnubbydApp();

  EXPECT_TRUE(extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(
      gnubbyd_app_id()));

  // No gnubbyd app item should exist in the shelf before the window is
  // launched.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(gnubbyd_app_id(), /*exists=*/false));

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->LaunchAppFromAppList(gnubbyd_app_id());

  // With the older ash which does not support app service block list, gnubbyd
  // app should not be published in app service, and can't be launched in app
  // list.
  // No gnubbyd item should exist in the shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(gnubbyd_app_id(), /*exists=*/false));

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());
}

// This tests the GCSE extension (running in both ash and lacros) should be
// rejected by ForWhichExtensionType, i.e., returning false for Matches()), with
// ash which supports app service block list.
// TODO(crbug.com/1409199): Remove the fake GCSE extension, and configure ash
// with a testing extension to exercise the test case.
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       GCSEExtensionNotMatchWithBlocklistSupport) {
  if (!extensions::IsAppServiceBlocklistCrosapiSupported()) {
    GTEST_SKIP() << "Test should not run with old ash version";
  }

  ForWhichExtensionType for_which_type =
      ForWhichExtensionType(InitForExtensions());

  InstallFakeGCSEExtension();

  EXPECT_TRUE(extensions::ExtensionRunsInBothOSAndStandaloneBrowser(
      gcse_extension()->id()));
  EXPECT_FALSE(for_which_type.Matches(gcse_extension()));
}

// This tests the GCSE extension (running in both ash and lacros) should be
// rejected by ForWhichExtensionType, i.e., returning false for Matches()), with
// the older ash which does not support app service block list.
// This verifies the fix for crbug.com/1408982.
// TODO(crbug.com/1409199): Remove the fake GCSE extension, and configure ash
// with a testing extension to exercise the test case.
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       GCSEExtensionNotMatchWithoutBlocklistSupport) {
  if (extensions::IsAppServiceBlocklistCrosapiSupported()) {
    GTEST_SKIP() << "Test should not run with new ash version";
  }

  ForWhichExtensionType for_which_type =
      ForWhichExtensionType(InitForExtensions());

  InstallFakeGCSEExtension();

  EXPECT_TRUE(extensions::ExtensionRunsInBothOSAndStandaloneBrowser(
      gcse_extension()->id()));
  EXPECT_FALSE(for_which_type.Matches(gcse_extension()));
}

}  // namespace extensions

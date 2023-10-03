// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
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

namespace {

const char kExtensionRunInAshAndLacrosId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kExtensionAppRunInAshAndLacrosId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kExtensionRunInAshOnlyId[] = "cccccccccccccccccccccccccccc";
const char kExtensionAppRunInAshOnlyId[] = "dddddddddddddddddddddddddddd";

const char kTestExtensionId[] = "pkplfbidichfdicaijlchgnapepdginl";
const char kTestChromeAppId[] = "knldjmfmopnpolahpmmgbagdohdnhkik";
}  // namespace

using LacrosExtensionKeeplistTest = ExtensionApiTest;

// Tests that Ash extension keeplist data is passed from Ash to Lacros via
// crosapi::mojom::BrowserInitParams.
IN_PROC_BROWSER_TEST_F(LacrosExtensionKeeplistTest,
                       AshKeeplistFromBrowserInitParams) {
  // Verify Ash extension keeplist data is passed to Lacros from Ash via
  // crosapi::mojom::BrowserInitParams, and do some minimum sanity check to make
  // sure the extension list passed from Ash is not empty. We have a more
  // sophiscaited test in extension_keeplist_ash_browsertest.cc to verify the
  // keep lists are idnetical in Ash and Lacros for such case.
  ASSERT_FALSE(
      chromeos::BrowserParamsProxy::Get()->ExtensionKeepList().is_null());
  EXPECT_FALSE(extensions::GetExtensionsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(
      extensions::GetExtensionAppsRunInOSAndStandaloneBrowser().empty());
  EXPECT_FALSE(extensions::GetExtensionsRunInOSOnly().empty());
  EXPECT_FALSE(extensions::GetExtensionAppsRunInOSOnly().empty());
}

class ExtensionAppsAppServiceBlocklistTest
    : public extensions::ExtensionBrowserTest {
 public:
  void SetUp() override {
    // Start unique Ash instance and pass ids of testing extension and chrome
    // app for Ash Extension Keeplist in additional Ash commandline switches.
    StartUniqueAshChrome(
        /*enabled_features=*/{}, /*disabled_features=*/{},
        {base::StringPrintf("extensions-run-in-ash-and-lacros=%s",
                            kTestExtensionId),
         base::StringPrintf("extension-apps-run-in-ash-and-lacros=%s",
                            kTestChromeAppId),
         base::StringPrintf("extension-apps-block-for-app-service-in-ash=%s",
                            kTestChromeAppId)},
        "crbug/1409199 test ash keeplist");
    ExtensionBrowserTest::SetUp();
  }

  void InstallTestChromeApp() {
    DCHECK(test_app_id_.empty());
    const extensions::Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("ash_extension_keeplist/simple_app"));
    test_app_id_ = extension->id();
    EXPECT_EQ(test_app_id_, kTestChromeAppId);
  }

  void InstallTestExtension() {
    DCHECK(!test_extension_);
    test_extension_ = LoadExtension(
        test_data_dir_.AppendASCII("ash_extension_keeplist/simple_extension"));
    EXPECT_EQ(test_extension_->id(), kTestExtensionId);
  }

  const std::string& test_app_id() const { return test_app_id_; }
  const extensions::Extension* test_extension() { return test_extension_; }

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
    if (!test_app_id_.empty()) {
      ASSERT_TRUE(browser_test_util::WaitForShelfItem(test_app_id_,
                                                      /*exists=*/false));
    }
  }

  std::string test_app_id_;
  raw_ptr<const extensions::Extension> test_extension_ = nullptr;
};

// This tests publishing and launching the test app (running in both ash and
// lacros, but only published to App Service in Lacros) with app service.
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       TestAppLaunchInAppList) {
  CHECK(extensions::IsAppServiceBlocklistCrosapiSupported());

  // Create the controller and publisher.
  std::unique_ptr<LacrosExtensionAppsPublisher> publisher =
      LacrosExtensionAppsPublisher::MakeForChromeApps();
  publisher->Initialize();
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Initialize(publisher->publisher());

  // Install the testing chrome app in Lacros.
  InstallTestChromeApp();

  // TODO(crbug/1459375): Install the testing chrome app in Ash and make sure
  // it is not published to App Service in Ash. Since we don't have a convenient
  // way to install an extension app in Ash from Lacros browser test, we will
  // defer that until crbug/1459375 is fixed.

  EXPECT_TRUE(
      extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(test_app_id()));
  EXPECT_FALSE(
      extensions::ExtensionAppBlockListedForAppServiceInStandaloneBrowser(
          test_app_id()));

  // The test chrome app item should not exist in the shelf before the app is
  // launched.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(test_app_id(), /*exists=*/false));

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // The test app should have been published in app service by lacros,
  // and can be launched from app list.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->LaunchAppFromAppList(test_app_id());

  // Wait for item to exist in shelf.
  ASSERT_TRUE(
      browser_test_util::WaitForShelfItem(test_app_id(), /*exists=*/true));
}

// This tests the test extension (running in both ash and lacros, but not
// published to app service) should be rejected by ForWhichExtensionType, i.e.,
// returning false for Matches()).
IN_PROC_BROWSER_TEST_F(ExtensionAppsAppServiceBlocklistTest,
                       ExtensionNotMatch) {
  CHECK(extensions::IsAppServiceBlocklistCrosapiSupported());

  ForWhichExtensionType for_which_type =
      ForWhichExtensionType(InitForExtensions());

  InstallTestExtension();
  EXPECT_TRUE(extensions::ExtensionRunsInBothOSAndStandaloneBrowser(
      test_extension()->id()));
  EXPECT_FALSE(for_which_type.Matches(test_extension()));
}

class KeeplistIdsFromAshCmdlineSwitchTest
    : public extensions::ExtensionBrowserTest {
 public:
  void SetUp() override {
    // Start unique Ash instance and pass ids of testing extensions and chrome
    // apps for Ash Extension Keeplist in the additional Ash commandline
    // switches.
    StartUniqueAshChrome(
        /*enabled_features=*/{}, /*disabled_features=*/{},
        {base::StringPrintf("extensions-run-in-ash-and-lacros=%s",
                            kExtensionRunInAshAndLacrosId),
         base::StringPrintf("extension-apps-run-in-ash-and-lacros=%s",
                            kExtensionAppRunInAshAndLacrosId),
         base::StringPrintf("extensions-run-in-ash-only=%s",
                            kExtensionRunInAshOnlyId),
         base::StringPrintf("extension-apps-run-in-ash-only=%s",
                            kExtensionAppRunInAshOnlyId)},
        "crbug/1371250 extension and chrome app running in both ash and "
        "lacros");
    ExtensionBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(KeeplistIdsFromAshCmdlineSwitchTest, GetTestIds) {
  EXPECT_TRUE(
      ExtensionRunsInBothOSAndStandaloneBrowser(kExtensionRunInAshAndLacrosId));
  EXPECT_TRUE(ExtensionRunsInOS(kExtensionRunInAshAndLacrosId));
  EXPECT_TRUE(ExtensionAppRunsInBothOSAndStandaloneBrowser(
      kExtensionAppRunInAshAndLacrosId));
  EXPECT_TRUE(ExtensionAppRunsInOS(kExtensionAppRunInAshAndLacrosId));
  EXPECT_TRUE(ExtensionRunsInOSOnly(kExtensionRunInAshOnlyId));
  EXPECT_TRUE(ExtensionRunsInOS(kExtensionRunInAshOnlyId));
  EXPECT_TRUE(ExtensionAppRunsInOSOnly(kExtensionAppRunInAshOnlyId));
  EXPECT_TRUE(ExtensionAppRunsInOS(kExtensionAppRunInAshOnlyId));
}

}  // namespace extensions

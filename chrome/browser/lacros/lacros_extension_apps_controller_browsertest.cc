// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include "base/containers/contains.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class LacrosExtensionAppsControllerTest
    : public extensions::ExtensionBrowserTest {
 public:
  void SetUp() override {
    // Since the browser tests run without Ash, Lacros won't get the Ash
    // extension keeplist data from Ash (passed via crosapi). Therefore,
    // set empty ash keeplist for test.
    extensions::SetEmptyAshKeeplistForTest();
    ExtensionBrowserTest::SetUp();
  }

  content::WebContents* GetActiveWebContents() {
    Browser* browser =
        chrome::FindTabbedBrowser(profile(), /*match_original_profiles=*/false);
    return browser->tab_strip_model()->GetActiveWebContents();
  }
};

// Test opening native settings for the app.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, OpenNativeSettings) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));

  // It doesn't matter what the URL is, it shouldn't be related to the
  // extension.
  ASSERT_FALSE(base::Contains(GetActiveWebContents()->GetVisibleURL().spec(),
                              extension->id()));

  // Send the message to open native settings.
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->OpenNativeSettings(extension->id());

  // Now the URL should be on a settings page that has the extension id.
  ASSERT_TRUE(base::Contains(GetActiveWebContents()->GetVisibleURL().spec(),
                             extension->id()));
}

// Test uninstalling an app.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, Uninstall) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));

  // Store the extension id since it will be uninstalled.
  std::string extension_id = extension->id();

  // Check that the app is installed.
  {
    const extensions::Extension* installed_extension =
        lacros_extensions_util::MaybeGetExtension(profile(), extension_id);
    EXPECT_TRUE(installed_extension && installed_extension->is_platform_app());
  }

  // Uninstall the extension.
  std::unique_ptr<LacrosExtensionAppsController> controller =
      LacrosExtensionAppsController::MakeForChromeApps();
  controller->Uninstall(extension->id(), apps::UninstallSource::kAppList,
                        /*clear_site_data=*/true,
                        /*report_abuse=*/true);

  // Check that the app is no longer installed.
  EXPECT_FALSE(
      lacros_extensions_util::MaybeGetExtension(profile(), extension_id));
}

// Test loading an icon
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, GetCompressedIcon) {
  const extensions::Extension* extension_minimal =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  const extensions::Extension* extension_with_icon =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/app_icon"));

  // For loop is much easier to set up then a doubly parameterized test.
  for (int i = 0; i <= 1; ++i) {
    // Regardless of whether we use an extension with an custom icon or not, an
    // icon should always load.
    const extensions::Extension* extension =
        (i == 0) ? extension_minimal : extension_with_icon;

    // Check that the compressed images load correctly.

    // Set up the LoadIconCallback which quits the nested run loop and
    // populates |output|.
    apps::IconValuePtr output;
    base::RunLoop run_loop;
    apps::LoadIconCallback callback = base::BindOnce(
        [](base::RunLoop* run_loop, apps::IconValuePtr* output,
           apps::IconValuePtr input) {
          *output = std::move(input);
          run_loop->QuitClosure().Run();
        },
        &run_loop, &output);

    // Load the icon
    std::unique_ptr<LacrosExtensionAppsController> controller =
        LacrosExtensionAppsController::MakeForChromeApps();
    controller->GetCompressedIcon(extension->id(), /*size_in_dip=*/1,
                                  ui::ResourceScaleFactor::k100Percent,
                                  std::move(callback));
    run_loop.Run();

    EXPECT_FALSE(output->compressed.empty());
  }
}

}  // namespace

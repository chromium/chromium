// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {
const char kWallpaperManagerExtensionID[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";
}  // namespace

class WallpaperManagerBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  WallpaperManagerBrowserTest();
  ~WallpaperManagerBrowserTest() override;

 protected:
  void VerifyWallpaperManagerLoaded();

 private:
  void LoadAndLaunchWallpaperManager();
};

WallpaperManagerBrowserTest::WallpaperManagerBrowserTest() {
}

WallpaperManagerBrowserTest::~WallpaperManagerBrowserTest() {
}

void WallpaperManagerBrowserTest::LoadAndLaunchWallpaperManager() {
  extension_service()->component_loader()->Add(
      IDR_WALLPAPERMANAGER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
  const Extension* wallpaper_app =
      ExtensionRegistry::Get(profile())->GetExtensionById(
          kWallpaperManagerExtensionID, ExtensionRegistry::EVERYTHING);
  LaunchPlatformApp(wallpaper_app);
}

void WallpaperManagerBrowserTest::VerifyWallpaperManagerLoaded() {
  ExtensionTestMessageListener window_created_listener(
      "wallpaper-window-created");
  ExtensionTestMessageListener launched_listener("launched");
  LoadAndLaunchWallpaperManager();
  EXPECT_TRUE(window_created_listener.WaitUntilSatisfied())
      << "Wallpaper picker window was not created.";
  EXPECT_TRUE(launched_listener.WaitUntilSatisfied())
      << "Wallpaper picker was not loaded.";
}

// Test for crbug.com/410550.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, DevLaunchApp) {
  VerifyWallpaperManagerLoaded();
}

// Test for crbug.com/410550. Wallpaper picker should be able to create
// alpha enabled window successfully.
IN_PROC_BROWSER_TEST_F(WallpaperManagerBrowserTest, StableLaunchApp) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  VerifyWallpaperManagerLoaded();
}

class WallpaperManagerJsTest : public InProcessBrowserTest {
 public:
  void RunTest(const base::FilePath& file) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(
            FILE_PATH_LITERAL("chromeos/wallpaper_manager/unit_tests")),
        file);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents));
  }
};

IN_PROC_BROWSER_TEST_F(WallpaperManagerJsTest, EventPageTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("event_page_unittest.html")));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_url_handler.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/system_web_apps/apps/os_url_handler_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class OsUrlHandlerTest : public ash::SystemWebAppBrowserTestBase {
 public:
  OsUrlHandlerTest() {
    OsUrlHandlerSystemWebAppDelegate::EnableDelegateForTesting(true);
  }
  ~OsUrlHandlerTest() override {
    OsUrlHandlerSystemWebAppDelegate::EnableDelegateForTesting(false);
  }
};

IN_PROC_BROWSER_TEST_F(OsUrlHandlerTest, Basic) {
  WaitForTestSystemAppInstall();

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: Url belongs to capturing system web app.
  EXPECT_FALSE(
      ash::TryLaunchOsUrlHandler(GURL(chrome::kChromeUIOSSettingsURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: Url is not allow-listed.
  EXPECT_FALSE(ash::TryLaunchOsUrlHandler(GURL("http://google.com")));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Success.
  const GURL url(chrome::kChromeUITermsURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  EXPECT_EQ(0u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
  EXPECT_TRUE(ash::TryLaunchOsUrlHandler(url));
  observer.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  EXPECT_EQ(1u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
}

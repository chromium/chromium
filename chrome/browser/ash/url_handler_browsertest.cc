// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/url_handler.h"

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/system_web_apps/apps/os_url_handler_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class UrlHandlerTest : public ash::SystemWebAppBrowserTestBase {
 public:
  explicit UrlHandlerTest(bool enable_lacros = true) {
    if (enable_lacros) {
      scoped_feature_list_.InitWithFeatures(
          ash::standalone_browser::GetFeatureRefs(), {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, ash::standalone_browser::GetFeatureRefs());
    }
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppBrowserTestBase::SetUpOnMainThread();
    if (browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work.
      // TODO(crbug.com/1450158): Remove uses of browser() from
      // SystemWebAppBrowserTestBase.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      SelectFirstBrowser();
    }
    WaitForTestSystemAppInstall();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UrlHandlerTest, Basic) {
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: terminal.
  EXPECT_FALSE(ash::TryOpenUrl(GURL("chrome-untrusted://terminal/"),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: media-app popup.
  EXPECT_FALSE(ash::TryOpenUrl(GURL("chrome-untrusted://media-app/"),
                               WindowOpenDisposition::NEW_POPUP));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: non-allowlisted non-SWA chrome page.
  EXPECT_FALSE(ash::TryOpenUrl(GURL(chrome::kChromeUIDownloadsURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: external page.
  EXPECT_TRUE(ash::TryOpenUrl(GURL("https://google.com"),
                              WindowOpenDisposition::NEW_WINDOW));
  base::RunLoop().RunUntilIdle();
  // Routed to Lacros, hence no new Ash window.
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: allow-listed non-SWA chrome page.
  const GURL url(chrome::kChromeUITermsURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  EXPECT_EQ(0u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
  EXPECT_TRUE(ash::TryOpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB));
  observer.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  EXPECT_EQ(1u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
}

IN_PROC_BROWSER_TEST_F(UrlHandlerTest, ChromeSchemeSemanticsLacros) {
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: ChromeSchemeSemantics::kLacros and chrome:// URL
  const GURL url(chrome::kChromeUIDownloadsURL);
  EXPECT_TRUE(ash::TryOpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
                              NavigateParams::RESPECT,
                              ash::ChromeSchemeSemantics::kLacros));
  base::RunLoop().RunUntilIdle();
  // Routed to Lacros, hence no new Ash window.
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(UrlHandlerTest, ManagementURL) {
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: allow-listed non-SWA chrome page.
  const GURL url(chrome::kChromeUIManagementURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  EXPECT_EQ(0u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
  EXPECT_TRUE(ash::TryOpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB));
  observer.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  EXPECT_EQ(1u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
}

IN_PROC_BROWSER_TEST_F(UrlHandlerTest, OsCreditsURL) {
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: allow-listed non-SWA chrome page.
  const GURL url(chrome::kChromeUIOSCreditsURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  EXPECT_EQ(0u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
  EXPECT_TRUE(ash::TryOpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB));
  observer.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  EXPECT_EQ(1u,
            GetSystemWebAppBrowserCount(ash::SystemWebAppType::OS_URL_HANDLER));
}

IN_PROC_BROWSER_TEST_F(UrlHandlerTest, SystemWebApp) {
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Success: OS Settings SWA.
  const GURL url(chrome::kChromeUIOSSettingsURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();
  EXPECT_EQ(0u, GetSystemWebAppBrowserCount(ash::SystemWebAppType::SETTINGS));
  EXPECT_TRUE(ash::TryOpenUrl(url, WindowOpenDisposition::NEW_FOREGROUND_TAB));
  observer.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
  EXPECT_EQ(1u, GetSystemWebAppBrowserCount(ash::SystemWebAppType::SETTINGS));
}

class UrlHandlerTestWithoutLacros : public UrlHandlerTest {
 public:
  UrlHandlerTestWithoutLacros() : UrlHandlerTest(/*enable_lacros=*/false) {}
};

IN_PROC_BROWSER_TEST_F(UrlHandlerTestWithoutLacros, Basic) {
  ASSERT_TRUE(crosapi::browser_util::IsAshWebBrowserEnabled());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: Lacros is disabled.
  EXPECT_FALSE(ash::TryOpenUrl(GURL(chrome::kChromeUISettingsURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Failure: Lacros is disabled.
  EXPECT_FALSE(ash::TryOpenUrl(GURL(chrome::kChromeUITermsURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

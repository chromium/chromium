// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

// TODO(crbug.com/1334173): Add tests to verify the scope and the start url are
// used as the ukm key when a webpage has a manifest.

namespace apps {

class WebsiteMetricsBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(profile);
    app_platform_metrics_service_ = metrics_service_.get();
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    DCHECK(proxy);
    proxy->SetAppPlatformMetricsServiceForTesting(std::move(metrics_service_));

    app_platform_metrics_service_->Start(proxy->AppRegistryCache(),
                                         proxy->InstanceRegistry());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest ::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  Browser* CreateBrowser() {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    Browser::CreateParams params(profile, true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  content::WebContents* NavigateAndWait(Browser* browser,
                                        const std::string& url,
                                        WindowOpenDisposition disposition) {
    NavigateParams params(browser, GURL(url),
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = disposition;
    Navigate(&params);
    auto* contents = params.navigated_or_inserted_contents;
    DCHECK_EQ(chrome::FindBrowserWithWebContents(
                  params.navigated_or_inserted_contents),
              browser);
    content::TestNavigationObserver observer(contents);
    observer.Wait();
    return contents;
  }

  void NavigateActiveTab(Browser* browser, const std::string& url) {
    NavigateAndWait(browser, url, WindowOpenDisposition::CURRENT_TAB);
  }

  content::WebContents* InsertForegroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }

  web_app::AppId InstallWebApp(const std::string& start_url,
                               web_app::UserDisplayMode user_display_mode) {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = GURL(start_url);
    info->user_display_mode = user_display_mode;
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto app_id = web_app::test::InstallWebApp(profile, std::move(info));
    return app_id;
  }

  web_app::AppId InstallWebAppOpeningAsTab(const std::string& start_url) {
    return InstallWebApp(start_url, web_app::UserDisplayMode::kBrowser);
  }

  WebsiteMetrics* website_metrics() {
    DCHECK(app_platform_metrics_service_);
    return app_platform_metrics_service_->website_metrics_.get();
  }

  base::flat_map<aura::Window*, content::WebContents*>&
  window_to_web_contents() {
    return website_metrics()->window_to_web_contents_;
  }

  base::flat_map<content::WebContents*,
                 std::unique_ptr<WebsiteMetrics::ActiveTabWebContentsObserver>>&
  webcontents_to_observer_map() {
    return website_metrics()->webcontents_to_observer_map_;
  }

  std::map<content::WebContents*, GURL>& webcontents_to_ukm_key() {
    return website_metrics()->webcontents_to_ukm_key_;
  }

 protected:
  AppPlatformMetricsService* app_platform_metrics_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, InsertAndCloseTabs) {
  InstallWebAppOpeningAsTab("https://a.example.org");

  Browser* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(1u, window_to_web_contents().size());

  // Insert an app tab.
  InsertForegroundTab(browser, "https://a.example.org");
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://a.example.org"));
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  // Open a second tab in foreground with no app.
  auto* tab_app1 = InsertForegroundTab(browser, "https://b.example.org");
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://b.example.org"));

  // Open two more tabs in foreground and close them.
  auto* tab_app3 = InsertForegroundTab(browser, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser, "https://d.example.org");

  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(3u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));

  // Close in reverse order.
  int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));

  i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app3);
  browser->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app3));

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, ForegroundTabNavigate) {
  Browser* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(1u, window_to_web_contents().size());

  // Open a tab in foreground.
  auto* tab_app = InsertForegroundTab(browser, "https://a.example.org");
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://a.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app], GURL("https://a.example.org"));

  // Navigate the foreground tab to a different url.
  NavigateActiveTab(browser, "https://b.example.org");

  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app], GURL("https://b.example.org"));

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, MultipleBrowser) {
  // Setup: two browsers with two tabs each.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab_app1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* tab_app2 = InsertForegroundTab(browser1, "https://b.example.org");

  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app2], GURL("https://b.example.org"));

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* tab_app3 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser2, "https://d.example.org");

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(4u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));

  // Close tabs.
  int i = browser1->tab_strip_model()->GetIndexOfWebContents(tab_app1);
  browser1->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(3u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app1));

  i = browser2->tab_strip_model()->GetIndexOfWebContents(tab_app3);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app3));

  i = browser2->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));

  i = browser1->tab_strip_model()->GetIndexOfWebContents(tab_app2);
  browser1->tab_strip_model()->CloseWebContentsAt(
      i, TabStripModel::CLOSE_USER_GESTURE);

  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, OnURLsDeleted) {
  // Setup: two browsers with one tabs each.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab_app1 = InsertForegroundTab(browser1, "https://a.example.org");

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* tab_app2 = InsertForegroundTab(browser2, "https://b.example.org");

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://a.example.org"));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app2], GURL("https://b.example.org"));

  // Simulate OnURLsDeleted is called.
  website_metrics()->OnURLsDeleted(nullptr,
                                   history::DeletionInfo::ForAllHistory());
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  // Create 2 tabs for the 2 browsers separately.
  auto* tab_app3 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser2, "https://d.example.org");

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://c.example.org"));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));

  // Close the browsers.
  browser1->tab_strip_model()->CloseAllTabs();
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
}

}  // namespace apps

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_types.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"

namespace apps {

class TestWebsiteMetrics : public WebsiteMetrics {
 public:
  explicit TestWebsiteMetrics(Profile* profile) : WebsiteMetrics(profile) {}

  void AwaitForInstallableWebAppCheck(const GURL& ukm_key) {
    if (on_checked_) {
      return;
    }

    ukm_key_ = ukm_key;

    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void OnWebContentsUpdated(content::WebContents* web_contents) override {
    WebsiteMetrics::OnWebContentsUpdated(web_contents);
    on_checked_ = false;
  }

  void OnInstallableWebAppStatusUpdated(
      content::WebContents* web_contents) override {
    WebsiteMetrics::OnInstallableWebAppStatusUpdated(web_contents);
    if (webcontents_to_ukm_key_[web_contents] != ukm_key_) {
      return;
    }

    on_checked_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
  bool on_checked_ = false;
  GURL ukm_key_;
};

class WebsiteMetricsBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());

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

  Browser* CreateAppBrowser(const std::string& app_id) {
    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    auto params = Browser::CreateParams::CreateForApp(
        "_crx_" + app_id, true /* trusted_source */,
        gfx::Rect(), /* window_bounts */
        profile, true /* user_gesture */);
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

  content::WebContents* InsertBackgroundTab(Browser* browser,
                                            const std::string& url) {
    return NavigateAndWait(browser, url,
                           WindowOpenDisposition::NEW_BACKGROUND_TAB);
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

  web_app::AppId InstallWebAppOpeningAsWindow(const std::string& start_url) {
    return InstallWebApp(start_url, web_app::UserDisplayMode::kStandalone);
  }

  void VerifyUrlInfo(const GURL& url,
                     UrlContent url_content,
                     bool is_activated,
                     bool promotable) {
    EXPECT_EQ(url_content, url_infos()[url].url_content);
    EXPECT_EQ(is_activated, url_infos()[url].is_activated);
    EXPECT_EQ(promotable, url_infos()[url].promotable);
  }

  void VerifyUrlInfoInPref(const GURL& url,
                           UrlContent url_content,
                           bool promotable) {
    DictionaryPrefUpdate update(
        ProfileManager::GetPrimaryUserProfile()->GetPrefs(), kWebsiteUsageTime);
    auto& dict = update->GetDict();

    const auto* url_info = dict.FindDict(url.spec());
    ASSERT_TRUE(url_info);
    auto url_content_value = url_info->FindInt(kUrlContentKey);
    ASSERT_TRUE(url_content_value.has_value());
    EXPECT_EQ(static_cast<int>(url_content), url_content_value.value());

    auto promotable_value = url_info->FindBool(kPromotableKey);
    ASSERT_TRUE(promotable_value.has_value());
    EXPECT_EQ(promotable, promotable_value.value());
  }

  void VerifyNoUrlInfoInPref(const GURL& url) {
    DictionaryPrefUpdate update(
        ProfileManager::GetPrimaryUserProfile()->GetPrefs(), kWebsiteUsageTime);
    auto& dict = update->GetDict();

    const auto* url_info = dict.FindDict(url.spec());
    ASSERT_FALSE(url_info);
  }

  void VerifyNoUsageTimeUkm(const GURL& url) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOS.WebsiteUsageTime");
    int count = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != url) {
        continue;
      }
      ++count;
    }
    ASSERT_EQ(0, count);
  }

  void VerifyUsageTimeUkm(const GURL& url,
                          UrlContent url_content,
                          bool promotable) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOS.WebsiteUsageTime");
    int count = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != url) {
        continue;
      }
      ++count;
      test_ukm_recorder()->ExpectEntryMetric(entry, "UrlContent",
                                             (int)url_content);
      test_ukm_recorder()->ExpectEntryMetric(entry, "Promotable", promotable);
    }
    ASSERT_EQ(1, count);
  }

  WebsiteMetrics* website_metrics() {
    DCHECK(app_platform_metrics_service_);
    return app_platform_metrics_service_->website_metrics_.get();
  }

  base::flat_map<aura::Window*, content::WebContents*>&
  window_to_web_contents() {
    return website_metrics()->window_to_web_contents_;
  }

  std::map<content::WebContents*,
           std::unique_ptr<WebsiteMetrics::ActiveTabWebContentsObserver>>&
  webcontents_to_observer_map() {
    return website_metrics()->webcontents_to_observer_map_;
  }

  std::map<content::WebContents*, GURL>& webcontents_to_ukm_key() {
    return website_metrics()->webcontents_to_ukm_key_;
  }

  std::map<GURL, WebsiteMetrics::UrlInfo>& url_infos() {
    return website_metrics()->url_infos_;
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 protected:
  AppPlatformMetricsService* app_platform_metrics_service_ = nullptr;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
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
  EXPECT_TRUE(url_infos().empty());

  // Open a second tab in foreground with no app.
  auto* tab_app1 = InsertForegroundTab(browser, "https://b.example.org");
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://b.example.org"));
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Open two more tabs in foreground and close them.
  auto* tab_app3 = InsertForegroundTab(browser, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser, "https://d.example.org");

  EXPECT_EQ(4u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(3u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));
  VerifyUrlInfo(GURL("https://c.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Close in reverse order.
  int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));
  VerifyUrlInfo(GURL("https://c.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app3);
  browser->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app3));
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  EXPECT_EQ(3u, url_infos().size());
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  website_metrics()->OnFiveMinutes();
  VerifyNoUrlInfoInPref(GURL("https://a.example.org"));
  VerifyUrlInfoInPref(GURL("https://b.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://c.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://d.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyNoUsageTimeUkm(GURL("https://a.example.org"));
  VerifyUsageTimeUkm(GURL("https://b.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://c.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://d.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
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
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

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
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://a.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://b.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, NavigateToBackgroundTab) {
  auto website_metrics_ptr = std::make_unique<apps::TestWebsiteMetrics>(
      ProfileManager::GetPrimaryUserProfile());
  auto* metrics = website_metrics_ptr.get();
  app_platform_metrics_service_->SetWebsiteMetricsForTesting(
      std::move(website_metrics_ptr));

  Browser* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(1u, window_to_web_contents().size());
  // Open a tab in foreground.
  GURL url1 =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  auto* tab1 = InsertForegroundTab(browser, url1.spec());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab1], url1);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the background tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto ukm_key = url2.GetWithoutFilename();
  auto* tab2 = InsertBackgroundTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(ukm_key);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(), tab2));
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], ukm_key);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/false, /*promotable=*/true);

  website_metrics()->OnFiveMinutes();
  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/false, /*promotable=*/true);
  VerifyUrlInfoInPref(url1, UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyNoUrlInfoInPref(ukm_key);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1, UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyNoUsageTimeUkm(ukm_key);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, ActiveBackgroundTab) {
  auto website_metrics_ptr = std::make_unique<apps::TestWebsiteMetrics>(
      ProfileManager::GetPrimaryUserProfile());
  auto* metrics = website_metrics_ptr.get();
  app_platform_metrics_service_->SetWebsiteMetricsForTesting(
      std::move(website_metrics_ptr));

  Browser* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(1u, window_to_web_contents().size());
  // Open a tab in foreground.
  GURL url1 =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  auto* tab1 = InsertForegroundTab(browser, url1.spec());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab1], url1);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the background tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto ukm_key = url2.GetWithoutFilename();
  auto* tab2 = InsertBackgroundTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(ukm_key);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(), tab2));
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], ukm_key);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();

  browser->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url2);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/true, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1, UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(ukm_key, UrlContent::kScope,
                      /*promotable=*/true);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1, UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(ukm_key, UrlContent::kScope,
                     /*promotable=*/true);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, NavigateToUrlWithManifest) {
  auto website_metrics_ptr = std::make_unique<apps::TestWebsiteMetrics>(
      ProfileManager::GetPrimaryUserProfile());
  auto* metrics = website_metrics_ptr.get();
  app_platform_metrics_service_->SetWebsiteMetricsForTesting(
      std::move(website_metrics_ptr));

  Browser* browser = CreateBrowser();
  auto* window = browser->window()->GetNativeWindow();
  EXPECT_EQ(1u, window_to_web_contents().size());

  // Open a tab in foreground.
  GURL url1 =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  auto* tab_app = InsertForegroundTab(browser, url1.spec());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app], url1);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the foreground tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto ukm_key = url2.GetWithoutFilename();
  NavigateActiveTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(ukm_key);
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url2);
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app], ukm_key);
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/true, /*promotable=*/true);

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1, UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(ukm_key, UrlContent::kScope,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1, UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(ukm_key, UrlContent::kScope,
                      /*promotable=*/true);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1, UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(ukm_key, UrlContent::kScope,
                     /*promotable=*/true);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, MultipleBrowser) {
  // Setup: two browsers with two tabs each.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab_app1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* tab_app2 = InsertForegroundTab(browser1, "https://b.example.org");

  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app2], GURL("https://b.example.org"));
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* tab_app3 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser2, "https://d.example.org");

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(4u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(4u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://c.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  // Close tabs.
  int i = browser1->tab_strip_model()->GetIndexOfWebContents(tab_app1);
  browser1->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(3u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(3u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app1));
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  i = browser2->tab_strip_model()->GetIndexOfWebContents(tab_app3);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app3));
  VerifyUrlInfo(GURL("https://c.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);

  i = browser2->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  // Simulate the window's activated status is switched from `window2` to
  // `window1`.
  website_metrics()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      window1, window2);
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);

  i = browser1->tab_strip_model()->GetIndexOfWebContents(tab_app2);
  browser1->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://a.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://c.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://d.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://b.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://c.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://d.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, WindowedWebApp) {
  std::string app_id = InstallWebAppOpeningAsWindow("https://d.example.org");

  // Open app D in a window (configured to open in a window).
  Browser* browser = CreateAppBrowser(app_id);
  InsertForegroundTab(browser, "https://d.example.org");

  // Verify there is no window, web contents recorded.
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  EXPECT_TRUE(url_infos().empty());

  // Close the browser.
  browser->tab_strip_model()->CloseAllTabs();

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
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
  VerifyUrlInfo(GURL("https://a.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://a.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);

  // Simulate OnURLsDeleted is called.
  website_metrics()->OnURLsDeleted(nullptr,
                                   history::DeletionInfo::ForAllHistory());
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  EXPECT_TRUE(url_infos().empty());
  VerifyNoUrlInfoInPref(GURL("https://a.example.org"));
  VerifyNoUrlInfoInPref(GURL("https://b.example.org"));

  // Create 2 tabs for the 2 browsers separately.
  auto* tab_app3 = InsertForegroundTab(browser1, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser2, "https://d.example.org");

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(4u, webcontents_to_observer_map().size());
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://c.example.org"));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));
  VerifyUrlInfo(GURL("https://c.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"), UrlContent::kFullUrl,
                /*is_activated=*/true, /*promotable=*/false);
  website_metrics()->OnFiveMinutes();
  // "https://c.example.org" is inactivated, and the running time is zero, so it
  // won't be saved in the user pref.
  VerifyNoUrlInfoInPref(GURL("https://c.example.org"));
  VerifyUrlInfoInPref(GURL("https://d.example.org"), UrlContent::kFullUrl,
                      /*promotable=*/false);

  // Close the browsers.
  browser1->tab_strip_model()->CloseAllTabs();
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyNoUsageTimeUkm(GURL("https://a.example.org"));
  VerifyNoUsageTimeUkm(GURL("https://b.example.org"));
  VerifyNoUsageTimeUkm(GURL("https://c.example.org"));
  VerifyUsageTimeUkm(GURL("https://d.example.org"), UrlContent::kFullUrl,
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

}  // namespace apps

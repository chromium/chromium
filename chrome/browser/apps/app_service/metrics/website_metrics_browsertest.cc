// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include <memory>
#include <optional>
#include <set>

#include "ash/constants/web_app_id_constants.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_browser_test_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_types.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/window_util.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Eq;

namespace apps {

class TestWebsiteMetrics : public WebsiteMetrics {
 public:
  explicit TestWebsiteMetrics(Profile* profile)
      : WebsiteMetrics(profile,
                       /*user_type_by_device_type=*/0) {}

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
      content::WebContents* web_contents,
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override {
    WebsiteMetrics::OnInstallableWebAppStatusUpdated(web_contents, result,
                                                     data);
    if (webcontents_to_ukm_key_.find(web_contents) ==
            webcontents_to_ukm_key_.end() ||
        webcontents_to_ukm_key_[web_contents] != ukm_key_) {
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

// Mock observer for the `WebsiteMetrics` component used for testing purposes.
class MockObserver : public WebsiteMetrics::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnUrlOpened,
              (const GURL& gurl, ::content::WebContents* web_contents),
              (override));

  MOCK_METHOD(void,
              OnUrlClosed,
              (const GURL& gurl, ::content::WebContents* web_contents),
              (override));

  MOCK_METHOD(void,
              OnUrlUsage,
              (const GURL& gurl, base::TimeDelta running_time),
              (override));

  MOCK_METHOD(void, OnWebsiteMetricsDestroyed, (), (override));
};

class WebsiteMetricsBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kNoStartupWindow);
  }

  Browser* CreateBrowser() {
    return website_metrics_browser_test_mixin_.CreateBrowser();
  }

  Browser* CreateAppBrowser(const std::string& app_id) {
    auto params = Browser::CreateParams::CreateForApp(
        "_crx_" + app_id, true /* trusted_source */,
        gfx::Rect(), /* window_bounts */
        profile(), true /* user_gesture */);
    Browser* browser = Browser::Create(params);
    browser->window()->Show();
    return browser;
  }

  ::content::WebContents* InsertForegroundTab(Browser* browser,
                                              const std::string& url) {
    return website_metrics_browser_test_mixin_.InsertForegroundTab(browser,
                                                                   url);
  }

  ::content::WebContents* InsertBackgroundTab(Browser* browser,
                                              const std::string& url) {
    return website_metrics_browser_test_mixin_.InsertBackgroundTab(browser,
                                                                   url);
  }

  void NavigateActiveTab(Browser* browser, const std::string& url) {
    return website_metrics_browser_test_mixin_.NavigateActiveTab(browser, url);
  }

  webapps::AppId InstallWebApp(
      const std::string& start_url,
      web_app::mojom::UserDisplayMode user_display_mode) {
    auto info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL(start_url));
    info->user_display_mode = user_display_mode;
    auto app_id = web_app::test::InstallWebApp(profile(), std::move(info));
    return app_id;
  }

  webapps::AppId InstallWebAppOpeningAsTab(const std::string& start_url) {
    return InstallWebApp(start_url, web_app::mojom::UserDisplayMode::kBrowser);
  }

  webapps::AppId InstallWebAppOpeningAsWindow(const std::string& start_url) {
    return InstallWebApp(start_url,
                         web_app::mojom::UserDisplayMode::kStandalone);
  }

  void VerifyUrlInfo(const GURL& url, bool is_activated, bool promotable) {
    EXPECT_EQ(is_activated, url_infos()[url].is_activated);
    EXPECT_EQ(promotable, url_infos()[url].promotable);
  }

  void VerifyUrlInfoInPref(const GURL& url, bool promotable) {
    const auto& dict = profile()->GetPrefs()->GetDict(kWebsiteUsageTime);

    const auto* url_info = dict.FindDict(url.spec());
    ASSERT_TRUE(url_info);

    auto promotable_value = url_info->FindBool(kPromotableKey);
    ASSERT_TRUE(promotable_value.has_value());
    EXPECT_EQ(promotable, promotable_value.value());
  }

  void VerifyNoUrlInfoInPref(const GURL& url) {
    const auto& dict = profile()->GetPrefs()->GetDict(kWebsiteUsageTime);

    const auto* url_info = dict.FindDict(url.spec());
    ASSERT_FALSE(url_info);
  }

  void VerifyNoUsageTimeUkm(const GURL& url) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOS.WebsiteUsageTime");
    int count = 0;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != url) {
        continue;
      }
      ++count;
    }
    ASSERT_EQ(0, count);
  }

  void VerifyUsageTimeUkm(const GURL& url, bool promotable) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOS.WebsiteUsageTime");
    int count = 0;
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != url) {
        continue;
      }
      ++count;
      test_ukm_recorder()->ExpectEntryMetric(entry, "Promotable", promotable);
    }
    ASSERT_EQ(1, count);
  }

  base::flat_map<aura::Window*, raw_ptr<content::WebContents, CtnExperimental>>&
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

  Profile* profile() { return ProfileManager::GetPrimaryUserProfile(); }

  WebsiteMetrics* website_metrics() {
    return website_metrics_browser_test_mixin_.website_metrics();
  }

 protected:
  WebsiteMetricsBrowserTestMixin website_metrics_browser_test_mixin_{
      &mixin_host_};
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
                             window_to_web_contents()[window].get()));
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
  VerifyUrlInfo(GURL("https://b.example.org"),
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
  VerifyUrlInfo(GURL("https://c.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  // Close in reverse order.
  int i = browser->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));
  VerifyUrlInfo(GURL("https://c.example.org"),
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
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  EXPECT_EQ(3u, url_infos().size());
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  website_metrics()->OnFiveMinutes();
  VerifyNoUrlInfoInPref(GURL("https://a.example.org"));
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://c.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://d.example.org"),
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyNoUsageTimeUkm(GURL("https://a.example.org"));
  VerifyUsageTimeUkm(GURL("https://b.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://c.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://d.example.org"),
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
  VerifyUrlInfo(GURL("https://a.example.org"),
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
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://a.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://b.example.org"),
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, NavigateToBackgroundTab) {
  auto website_metrics_ptr =
      std::make_unique<apps::TestWebsiteMetrics>(profile());
  auto* const metrics = website_metrics_ptr.get();
  website_metrics_browser_test_mixin_.metrics_service()
      ->SetWebsiteMetricsForTesting(std::move(website_metrics_ptr));

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
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the background tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto* tab2 = InsertBackgroundTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(url2);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(), tab2));
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], url2);
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/true);

  website_metrics()->OnFiveMinutes();
  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/true);
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/false);
  VerifyNoUrlInfoInPref(url2);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1,
                     /*promotable=*/false);
  VerifyNoUsageTimeUkm(url2);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, ActiveBackgroundTab) {
  auto website_metrics_ptr =
      std::make_unique<apps::TestWebsiteMetrics>(profile());
  auto* const metrics = website_metrics_ptr.get();
  website_metrics_browser_test_mixin_.metrics_service()
      ->SetWebsiteMetricsForTesting(std::move(website_metrics_ptr));

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
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the background tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto* tab2 = InsertBackgroundTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(url2);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(), tab2));
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], url2);
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();

  browser->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url2);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/true, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(url2,
                      /*promotable=*/true);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(url2,
                     /*promotable=*/true);
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, NavigateToUrlWithManifest) {
  auto website_metrics_ptr =
      std::make_unique<apps::TestWebsiteMetrics>(profile());
  auto* const metrics = website_metrics_ptr.get();
  website_metrics_browser_test_mixin_.metrics_service()
      ->SetWebsiteMetricsForTesting(std::move(website_metrics_ptr));

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
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/false);

  // Navigate the foreground tab to a url with a manifest.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  NavigateActiveTab(browser, url2.spec());
  metrics->AwaitForInstallableWebAppCheck(url2);
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window]));
  EXPECT_EQ(window_to_web_contents()[window]->GetVisibleURL(), url2);
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app], url2);
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/true, /*promotable=*/true);

  browser->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/true);
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/false);
  VerifyUrlInfoInPref(url2,
                      /*promotable=*/true);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(url1,
                     /*promotable=*/false);
  VerifyUsageTimeUkm(url2,
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
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* tab_app3 = InsertForegroundTab(browser2, "https://c.example.org");
  auto* tab_app4 = InsertForegroundTab(browser2, "https://d.example.org");
  wm::GetActivationClient(window1->GetRootWindow())->DeactivateWindow(window1);

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(4u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://d.example.org"));
  EXPECT_EQ(4u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app3], GURL("https://c.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app4], GURL("https://d.example.org"));
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://c.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"),
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
  VerifyUrlInfo(GURL("https://d.example.org"),
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
  VerifyUrlInfo(GURL("https://c.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  i = browser2->tab_strip_model()->GetIndexOfWebContents(tab_app4);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  wm::GetActivationClient(window1->GetRootWindow())->ActivateWindow(window1);
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_FALSE(base::Contains(webcontents_to_ukm_key(), tab_app4));
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"),
                /*is_activated=*/false, /*promotable=*/false);

  i = browser1->tab_strip_model()->GetIndexOfWebContents(tab_app2);
  browser1->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://a.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://c.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://d.example.org"),
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://b.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://c.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(GURL("https://d.example.org"),
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

// TODO(crbug.com/40910130): Test is flaky.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_MoveActivatedTabToNewBrowser DISABLED_MoveActivatedTabToNewBrowser
#else
#define MAYBE_MoveActivatedTabToNewBrowser MoveActivatedTabToNewBrowser
#endif
IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest,
                       MAYBE_MoveActivatedTabToNewBrowser) {
  auto website_metrics_ptr =
      std::make_unique<apps::TestWebsiteMetrics>(profile());
  auto* const metrics = website_metrics_ptr.get();
  website_metrics_browser_test_mixin_.metrics_service()
      ->SetWebsiteMetricsForTesting(std::move(website_metrics_ptr));

  // Create a browser with two tabs.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();

  // Open a tab in foreground with a manifest.
  GURL url1 =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  auto* tab1 = InsertForegroundTab(browser1, url1.spec());
  metrics->AwaitForInstallableWebAppCheck(url1);
  // Open a background tab to a url.
  GURL url2 =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  auto* tab2 = InsertBackgroundTab(browser1, url2.spec());

  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab1], url1);
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], url2);
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/true);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/true);

  // Create the second browser, and move the activated tab to the new browser.
  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  wm::GetActivationClient(window1->GetRootWindow())->DeactivateWindow(window1);

  // Detach `tab1`.
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser1->tab_strip_model()->DetachTabAtForInsertion(0);

  // Attach `tab1` to `browser2`.
  browser2->tab_strip_model()->InsertDetachedTabAt(0, std::move(detached_tab),
                                                   AddTabTypes::ADD_ACTIVE);
  auto* tab3 = browser2->tab_strip_model()->GetWebContentsAt(0);

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(), url1);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab3], url1);
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], url2);
  VerifyUrlInfo(url1,
                /*is_activated=*/true, /*promotable=*/true);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/true);
  VerifyNoUrlInfoInPref(url2);

  auto* tab4 = InsertForegroundTab(browser2, "https://a.example.org");
  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(3u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://a.example.org"));
  EXPECT_EQ(3u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab4], GURL("https://a.example.org"));
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/true);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/false);

  auto i = browser2->tab_strip_model()->GetIndexOfWebContents(tab4);
  browser2->tab_strip_model()->CloseWebContentsAt(
      i, TabCloseTypes::CLOSE_USER_GESTURE);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnFiveMinutes();
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"),
                     /*promotable=*/false);
  VerifyUsageTimeUkm(url1,
                     /*promotable=*/true);
  VerifyNoUsageTimeUkm(url2);

  browser2->tab_strip_model()->CloseAllTabs();
  wm::GetActivationClient(window1->GetRootWindow())->ActivateWindow(window1);
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_TRUE(base::Contains(webcontents_to_ukm_key(), tab2));

  browser1->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(url1,
                /*is_activated=*/false, /*promotable=*/true);
  VerifyUrlInfo(url2,
                /*is_activated=*/false, /*promotable=*/false);

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(url1,
                      /*promotable=*/true);
  VerifyUrlInfoInPref(url2,
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  EXPECT_TRUE(url_infos().empty());
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest,
                       MoveInActivatedTabToNewBrowser) {
  // Create a browser with two tabs.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab1 = InsertForegroundTab(browser1, "https://a.example.org");
  auto* tab2 = InsertBackgroundTab(browser1, "https://b.example.org");

  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window1]));
  EXPECT_EQ(window_to_web_contents()[window1]->GetVisibleURL(),
            GURL("https://a.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab2], GURL("https://b.example.org"));
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/true, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://a.example.org"),
                      /*promotable=*/false);
  VerifyNoUrlInfoInPref(GURL("https://b.example.org"));

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://a.example.org"),
                     /*promotable=*/false);
  VerifyNoUsageTimeUkm(GURL("https://b.example.org"));

  // Create the second browser, and move the inactivated tab to the new browser.
  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  wm::GetActivationClient(window1->GetRootWindow())->DeactivateWindow(window1);

  // Detach `tab2`.
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser1->tab_strip_model()->DetachTabAtForInsertion(1);

  // Attach `tab2` to `browser2`.
  browser2->tab_strip_model()->InsertDetachedTabAt(0, std::move(detached_tab),
                                                   AddTabTypes::ADD_ACTIVE);
  auto* tab3 = browser2->tab_strip_model()->GetWebContentsAt(0);

  EXPECT_EQ(2u, window_to_web_contents().size());
  EXPECT_EQ(2u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(window_to_web_contents()[window2]->GetVisibleURL(),
            GURL("https://b.example.org"));
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab3], GURL("https://b.example.org"));
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
  VerifyUsageTimeUkm(GURL("https://b.example.org"),
                     /*promotable=*/false);

  browser1->tab_strip_model()->CloseAllTabs();
  EXPECT_EQ(1u, window_to_web_contents().size());
  EXPECT_EQ(1u, webcontents_to_observer_map().size());
  EXPECT_TRUE(base::Contains(webcontents_to_observer_map(),
                             window_to_web_contents()[window2]));
  EXPECT_EQ(1u, webcontents_to_ukm_key().size());
  EXPECT_TRUE(base::Contains(webcontents_to_ukm_key(), tab3));

  browser2->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/false, /*promotable=*/false);

  EXPECT_TRUE(window_to_web_contents().empty());
  EXPECT_TRUE(webcontents_to_observer_map().empty());
  EXPECT_TRUE(webcontents_to_ukm_key().empty());

  website_metrics()->OnFiveMinutes();
  VerifyNoUrlInfoInPref(GURL("https://a.example.org"));
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);

  // Simulate recording the UKMs to clear the local usage time records.
  website_metrics()->OnTwoHours();
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

IN_PROC_BROWSER_TEST_F(WebsiteMetricsBrowserTest, OnHistoryDeletions) {
  // Setup: two browsers with one tabs each.
  auto* browser1 = CreateBrowser();
  auto* window1 = browser1->window()->GetNativeWindow();
  auto* tab_app1 = InsertForegroundTab(browser1, "https://a.example.org");

  auto* browser2 = CreateBrowser();
  auto* window2 = browser2->window()->GetNativeWindow();
  auto* tab_app2 = InsertForegroundTab(browser2, "https://b.example.org");
  wm::GetActivationClient(window1->GetRootWindow())->DeactivateWindow(window1);

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
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  // Simulate OnHistoryDeletions is called for an expiration. Nothing should be
  // cleared.
  auto info = history::DeletionInfo(
      history::DeletionTimeRange(base::Time(), base::Time::Now()),
      /*is_from_expiration=*/true, {}, {}, std::optional<std::set<GURL>>());
  website_metrics()->OnHistoryDeletions(nullptr, info);
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
  VerifyUrlInfo(GURL("https://a.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://b.example.org"),
                /*is_activated=*/true, /*promotable=*/false);

  // Persist data to prefs and verify.
  website_metrics()->OnFiveMinutes();
  VerifyUrlInfoInPref(GURL("https://a.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);

  // Simulate OnHistoryDeletions again for an expiration. The prefs should not
  // be affected
  website_metrics()->OnHistoryDeletions(nullptr, info);
  EXPECT_EQ(2u, webcontents_to_ukm_key().size());
  EXPECT_EQ(2u, url_infos().size());
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app1], GURL("https://a.example.org"));
  EXPECT_EQ(webcontents_to_ukm_key()[tab_app2], GURL("https://b.example.org"));
  VerifyUrlInfoInPref(GURL("https://a.example.org"),
                      /*promotable=*/false);
  VerifyUrlInfoInPref(GURL("https://b.example.org"),
                      /*promotable=*/false);

  // Simulate OnHistoryDeletions for a non-expiration and ensure prefs and
  // in-memory usage data is cleared.
  website_metrics()->OnHistoryDeletions(nullptr,
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
  VerifyUrlInfo(GURL("https://c.example.org"),
                /*is_activated=*/false, /*promotable=*/false);
  VerifyUrlInfo(GURL("https://d.example.org"),
                /*is_activated=*/true, /*promotable=*/false);
  website_metrics()->OnFiveMinutes();
  // "https://c.example.org" is inactivated, and the running time is zero, so it
  // won't be saved in the user pref.
  VerifyNoUrlInfoInPref(GURL("https://c.example.org"));
  VerifyUrlInfoInPref(GURL("https://d.example.org"),
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
  VerifyUsageTimeUkm(GURL("https://d.example.org"),
                     /*promotable=*/false);
  EXPECT_TRUE(url_infos().empty());
}

class WebsiteMetricsObserverBrowserTest : public WebsiteMetricsBrowserTest {
 protected:
  void TearDownOnMainThread() override {
    // Unregister observer to prevent noise during teardown.
    website_metrics()->RemoveObserver(&observer_);
    WebsiteMetricsBrowserTest::TearDownOnMainThread();
  }

  MockObserver observer_;
};

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest, NotifyOnUrlOpened) {
  const std::string& kUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();

  website_metrics()->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnUrlOpened)
      .WillOnce([&](const GURL& url, ::content::WebContents* web_contents) {
        EXPECT_THAT(url, Eq(GURL(kUrl)));
        EXPECT_THAT(web_contents, Eq(window_to_web_contents()[window]));
      });
  InsertForegroundTab(browser, kUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyOnBackgroundUrlOpened) {
  const std::string& kUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  NavigateActiveTab(browser, kUrl);
  website_metrics()->AddObserver(&observer_);

  const std::string& kBackgroundUrl = "https://b.example.org";
  EXPECT_CALL(observer_, OnUrlOpened(GURL(kBackgroundUrl), _)).Times(1);
  InsertBackgroundTab(browser, kBackgroundUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyUrlOpenedClosedOnContentNavigation) {
  const std::string& kOldUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();
  NavigateActiveTab(browser, kOldUrl);
  website_metrics()->AddObserver(&observer_);

  // Navigate to a different URL and verify observer is notified.
  const std::string& kNewUrl = "https://b.example.org";
  ::content::WebContents* const active_web_contents =
      window_to_web_contents()[window];
  EXPECT_CALL(observer_, OnUrlOpened(GURL(kNewUrl), active_web_contents))
      .Times(1);
  EXPECT_CALL(observer_, OnUrlClosed(GURL(kOldUrl), active_web_contents))
      .Times(1);
  NavigateActiveTab(browser, kNewUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyUrlClosedOnContentNavigationToRegisteredWebApp) {
  const std::string& kWebAppUrl = "https://b.example.org";
  InstallWebAppOpeningAsTab(kWebAppUrl);

  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();
  const std::string& kOldUrl = "https://a.example.org";
  NavigateActiveTab(browser, kOldUrl);
  website_metrics()->AddObserver(&observer_);

  // Navigate to the web app and verify observer is notified of URL close.
  const auto window_it = window_to_web_contents().find(window);
  ASSERT_NE(window_it, window_to_web_contents().end());
  ::content::WebContents* const active_web_contents = window_it->second;
  EXPECT_CALL(observer_, OnUrlClosed(GURL(kOldUrl), active_web_contents))
      .Times(1);
  NavigateActiveTab(browser, kWebAppUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyUrlOpenedOnContentNavigationFromRegisteredWebApp) {
  const std::string& kWebAppUrl = "https://b.example.org";
  InstallWebAppOpeningAsTab(kWebAppUrl);
  website_metrics()->AddObserver(&observer_);

  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();
  NavigateActiveTab(browser, kWebAppUrl);

  // Navigate to the URL from the web app and verify observer is notified of URL
  // open.
  const std::string& kNewUrl = "https://a.example.org";
  const auto window_it = window_to_web_contents().find(window);
  ASSERT_NE(window_it, window_to_web_contents().end());
  ::content::WebContents* const active_web_contents = window_it->second;
  EXPECT_CALL(observer_, OnUrlOpened(GURL(kNewUrl), active_web_contents))
      .Times(1);
  NavigateActiveTab(browser, kNewUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       DoNotNotifyIfUrlAlreadyOpenInRenderFrame) {
  const std::string& kUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  NavigateActiveTab(browser, kUrl);
  website_metrics()->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnUrlOpened).Times(0);
  NavigateActiveTab(browser, kUrl);
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyUrlClosedOnTabClose) {
  const std::string& kUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();
  NavigateActiveTab(browser, kUrl);
  website_metrics()->AddObserver(&observer_);

  // Close the tab and verify observer is notified.
  EXPECT_CALL(observer_,
              OnUrlClosed(GURL(kUrl), window_to_web_contents()[window].get()))
      .Times(1);
  browser->tab_strip_model()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyUrlClosedOnWindowClose) {
  // Open URLs in two separate tabs.
  const std::string& kUrl1 = "https://a.example.org";
  const std::string& kUrl2 = "https://b.example.org";
  auto* const browser = CreateBrowser();
  auto* const window = browser->window()->GetNativeWindow();
  NavigateActiveTab(browser, kUrl1);
  InsertBackgroundTab(browser, kUrl2);
  website_metrics()->AddObserver(&observer_);

  // Simulate window closure and verify observer is notified accordingly.
  const std::string& kNewUrl = "https://b.example.org";
  EXPECT_CALL(observer_,
              OnUrlClosed(GURL(kUrl1), window_to_web_contents()[window].get()))
      .Times(1);
  EXPECT_CALL(observer_, OnUrlClosed(GURL(kUrl2), _)).Times(1);
  browser->tab_strip_model()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       NotifyOnWebsiteMetricsDestroyed) {
  // Test with a separate instance of website metrics so we do not affect
  // pre-existing test teardown fixtures.
  std::unique_ptr<WebsiteMetrics> owned_website_metrics =
      std::make_unique<WebsiteMetrics>(profile(),
                                       /*user_type_by_device_type=*/0);
  owned_website_metrics->AddObserver(&observer_);
  EXPECT_CALL(observer_, OnWebsiteMetricsDestroyed).Times(1);
  owned_website_metrics.reset();
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest, NotifyOnUrlUsage) {
  const std::string& kUrl = "https://a.example.org";
  auto* const browser = CreateBrowser();
  NavigateActiveTab(browser, kUrl);
  website_metrics()->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnUrlUsage)
      .WillOnce([&](const GURL& url, base::TimeDelta running_time) {
        EXPECT_THAT(url, Eq(GURL(kUrl)));
        EXPECT_TRUE(running_time.is_positive());
      });
  website_metrics()->OnFiveMinutes();
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest,
                       DoNotNotifyBackgroundUrlUsage) {
  const std::string& kUrl = "https://a.example.org";
  const std::string& kBackgroundUrl = "https://b.example.org";
  auto* const browser = CreateBrowser();
  NavigateActiveTab(browser, kUrl);
  InsertBackgroundTab(browser, kBackgroundUrl);
  website_metrics()->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnUrlUsage(GURL(kUrl), _)).Times(1);
  EXPECT_CALL(observer_, OnUrlUsage(GURL(kBackgroundUrl), _)).Times(0);
  website_metrics()->OnFiveMinutes();
}

IN_PROC_BROWSER_TEST_F(WebsiteMetricsObserverBrowserTest, NoUrlUsage) {
  CreateBrowser();
  website_metrics()->AddObserver(&observer_);

  // Verify observer is not notified because there is no web content usage.
  EXPECT_CALL(observer_, OnUrlUsage).Times(0);
  website_metrics()->OnFiveMinutes();
}

}  // namespace apps

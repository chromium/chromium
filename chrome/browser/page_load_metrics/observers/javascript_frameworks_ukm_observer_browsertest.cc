// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/foreground_duration_ukm_observer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

constexpr char kGatsbyJsPageLoad[] = "GatsbyPageLoad";
constexpr char kNextJsPageLoad[] = "NextJSPageLoad";
constexpr char kNuxtJsPageLoad[] = "NuxtJSPageLoad";
constexpr char kSapperPageLoad[] = "SapperPageLoad";
constexpr char kVuePressPageLoad[] = "VuePressPageLoad";
constexpr char kAngularPageLoad[] = "AngularPageLoad";
constexpr char kPreactPageLoad[] = "PreactPageLoad";
constexpr char kReactPageLoad[] = "ReactPageLoad";
constexpr char kSveltePageLoad[] = "SveltePageLoad";
constexpr char kVuePageLoad[] = "VuePageLoad";
constexpr char kDrupalPageLoad[] = "DrupalPageLoad";
constexpr char kJoomlaPageLoad[] = "JoomlaPageLoad";
constexpr char kShopifyPageLoad[] = "ShopifyPageLoad";
constexpr char kSquarespacePageLoad[] = "SquarespacePageLoad";
constexpr char kWixPageLoad[] = "WixPageLoad";
constexpr char kWordPressPageLoad[] = "WordPressPageLoad";
const std::vector<base::StringPiece> all_frameworks = {
    kGatsbyJsPageLoad,  kNextJsPageLoad,      kNuxtJsPageLoad,
    kSapperPageLoad,    kVuePressPageLoad,    kAngularPageLoad,
    kPreactPageLoad,    kReactPageLoad,       kSveltePageLoad,
    kVuePageLoad,       kDrupalPageLoad,      kJoomlaPageLoad,
    kShopifyPageLoad,   kSquarespacePageLoad, kWixPageLoad,
    kWordPressPageLoad,
};

}  // namespace

class JavascriptFrameworksUkmObserverBrowserTest : public InProcessBrowserTest {
 public:
  JavascriptFrameworksUkmObserverBrowserTest() = default;

  JavascriptFrameworksUkmObserverBrowserTest(
      const JavascriptFrameworksUkmObserverBrowserTest&) = delete;
  JavascriptFrameworksUkmObserverBrowserTest& operator=(
      const JavascriptFrameworksUkmObserverBrowserTest&) = delete;

  ~JavascriptFrameworksUkmObserverBrowserTest() override = default;
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  void StartHttpsServer(net::EmbeddedTestServer::ServerCertificate cert) {
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(cert);
    https_test_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_->Start());
  }
  void ExpectMetricValueForUrl(
      const GURL& url,
      base::StringPiece metric_name,
      const int expected_value,
      base::StringPiece entry_name =
          ukm::builders::JavascriptFrameworkPageLoad::kEntryName) {
    for (auto* entry : test_ukm_recorder_->GetEntriesByName(entry_name)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url) {
        test_ukm_recorder_->EntryHasMetric(entry, metric_name);
        test_ukm_recorder_->ExpectEntryMetric(entry, metric_name,
                                              expected_value);
      }
    }
  }
  void ExpectMetricCountForUrl(
      const GURL& url,
      base::StringPiece metric_name,
      const int expected_count,
      base::StringPiece entry_name =
          ukm::builders::JavascriptFrameworkPageLoad::kEntryName) {
    int count = 0;
    for (auto* entry : test_ukm_recorder_->GetEntriesByName(entry_name)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url &&
          test_ukm_recorder_->EntryHasMetric(entry, metric_name)) {
        count++;
      }
    }
    EXPECT_EQ(count, expected_count);
  }
  void CloseAllTabs() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }
  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

  void RunSingleFrameworkDetectionTest(const std::string& test_url,
                                       base::StringPiece framework_name) {
    page_load_metrics::PageLoadMetricsTestWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    waiter.AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
    GURL url = https_test_server()->GetURL(test_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    waiter.Wait();
    CloseAllTabs();
    RunFrameworkDetection(all_frameworks, framework_name, url);
  }

  void RunSingleFrameworkVersionDetectionTest(
      const std::string& test_url,
      base::StringPiece framework,
      absl::optional<std::pair<int, int>> expected_version) {
    page_load_metrics::PageLoadMetricsTestWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    waiter.AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
    GURL url = https_test_server()->GetURL(test_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    waiter.Wait();
    CloseAllTabs();
    ExpectMetricCountForUrl(
        url, framework, expected_version.has_value() ? 1 : 0,
        ukm::builders::Blink_JavaScriptFramework_Versions::kEntryName);
    if (expected_version.has_value()) {
      ExpectMetricValueForUrl(
          url, framework,
          ((expected_version.value().first & 0xff) << 8) |
              (expected_version.value().second & 0xff),
          ukm::builders::Blink_JavaScriptFramework_Versions::kEntryName);
    }
  }

  void RunNoFrameworkVersionNotDetectedTest(const std::string& test_url) {
    page_load_metrics::PageLoadMetricsTestWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    waiter.AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
    GURL url = https_test_server()->GetURL(test_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    waiter.Wait();
    CloseAllTabs();
    auto entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::Blink_JavaScriptFramework_Versions::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  void RunSingleFrameworkDetectionTestForFencedFrames(
      const std::string& test_url) {
    StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
    GURL mainframe_url = https_test_server()->GetURL("/english_page.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), mainframe_url));

    page_load_metrics::PageLoadMetricsTestWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    waiter.AddPageExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    GURL subframe_url = https_test_server()->GetURL(test_url);
    content::RenderFrameHost* subframe =
        fenced_frame_helper_.CreateFencedFrame(browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetPrimaryMainFrame(),
                                               subframe_url);
    EXPECT_NE(nullptr, subframe);
    waiter.Wait();
    CloseAllTabs();

    // No frameworks should be detected.
    for (base::StringPiece framework : all_frameworks) {
      ExpectMetricCountForUrl(mainframe_url, framework, 1);
      ExpectMetricValueForUrl(mainframe_url, framework, false);
    }
  }

 private:
  void RunFrameworkDetection(const std::vector<base::StringPiece>& frameworks,
                             base::StringPiece framework_name,
                             const GURL& url) {
    for (base::StringPiece framework : frameworks) {
      ExpectMetricCountForUrl(url, framework, 1);
      if (framework.compare(framework_name) == 0) {
        ExpectMetricValueForUrl(url, framework, true);
      } else {
        ExpectMetricValueForUrl(url, framework, false);
      }
    }
  }
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NoFrameworkDetected) {
  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/english_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter.Wait();
  CloseAllTabs();
  for (base::StringPiece framework : all_frameworks) {
    ExpectMetricCountForUrl(url, framework, 1);
    ExpectMetricValueForUrl(url, framework, false);
  }
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       GatsbyFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/gatsby_page.html",
                                  kGatsbyJsPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NextjsFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/nextjs_page.html",
                                  kNextJsPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NuxtjsFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/nuxtjs_page.html",
                                  kNuxtJsPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       SapperFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/sapper_page.html",
                                  kSapperPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       VuePressFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/vuepress_page.html",
                                  kVuePressPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       MultipleFrameworksDetected) {
  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  waiter.AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL(
      "/page_load_metrics/multiple_frameworks.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter.Wait();
  CloseAllTabs();
  struct {
    base::StringPiece name;
    const bool in_page;
  } expected_frameworks[] = {{kGatsbyJsPageLoad, true},
                             {kNextJsPageLoad, true},
                             {kNuxtJsPageLoad, true},
                             {kSapperPageLoad, false},
                             {kVuePressPageLoad, false}};
  for (const auto& framework : expected_frameworks) {
    ExpectMetricCountForUrl(url, framework.name, 1);
    ExpectMetricValueForUrl(url, framework.name, framework.in_page);
  }
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       AngularFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/angular_page.html",
                                  kAngularPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       PreactFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/preact_page.html",
                                  kPreactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected1) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react1_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected2) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react2_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected3) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react3_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected4) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react4_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected5) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react5_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ReactFrameworkDetected6) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/react6_page.html",
                                  kReactPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       SvelteFrameworkDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/svelte_page.html",
                                  kSveltePageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       VueFrameworkDetected1) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/vue1_page.html",
                                  kVuePageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       VueFrameworkDetected2) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/vue2_page.html",
                                  kVuePageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       VueFrameworkDetected3) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/vue3_page.html",
                                  kVuePageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       DrupalCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/drupal_page.html",
                                  kDrupalPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       JoomlaCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/joomla_page.html",
                                  kJoomlaPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       ShopifyCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/shopify_page.html",
                                  kShopifyPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       SquarespaceCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/squarespace_page.html",
                                  kSquarespacePageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       WixCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/wix_page.html",
                                  kWixPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       WordPressCMSDetected) {
  RunSingleFrameworkDetectionTest("/page_load_metrics/wordpress_page.html",
                                  kWordPressPageLoad);
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NoFrameworksDetectedInFencedFrame) {
  RunSingleFrameworkDetectionTestForFencedFrames(
      "/page_load_metrics/gatsby_page.html");
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       AngularVersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/angular.html",
      "AngularVersion", std::make_pair(14, 0));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       AngularClampedVersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/angular-clamped.html",
      "AngularVersion", std::make_pair(300, 4000));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       DrupalVersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/drupal.html",
      "DrupalVersion", std::make_pair(7, 0));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NextJSVersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/nextjs.html",
      "NextJSVersion", std::make_pair(13, 3));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       Vue2VersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/vue2.html", "VueVersion",
      std::make_pair(2, 1));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       Vue3VersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/vue3.html", "VueVersion",
      std::make_pair(3, 0));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       WordPressVersionDetected) {
  RunSingleFrameworkVersionDetectionTest(
      "/page_load_metrics/framework-version-detection/wordpress.html",
      "WordPressVersion", std::make_pair(6, 2));
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NoFrameworkVersionDetected) {
  RunNoFrameworkVersionNotDetectedTest(
      "/page_load_metrics/framework-version-detection/not-detected.html");
}

IN_PROC_BROWSER_TEST_F(JavascriptFrameworksUkmObserverBrowserTest,
                       NoFrameworkVersionDetectedBadValues) {
  RunNoFrameworkVersionNotDetectedTest(
      "/page_load_metrics/framework-version-detection/"
      "not-detected-bad-values.html");
}

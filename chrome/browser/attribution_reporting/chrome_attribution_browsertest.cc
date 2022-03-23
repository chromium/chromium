// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for the Conversion Measurement API that rely on chrome/ layer features.
// UseCounter recording and multiple browser window behavior is not available
// content shell.
class ChromeAttributionBrowserTest : public InProcessBrowserTest {
 public:
  ChromeAttributionBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&server_);
    server_.ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(&server_);
    ASSERT_TRUE(server_.Start());
  }

 protected:
  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       ImpressionClicked_FeatureRecorded) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  page_load_metrics::PageLoadMetricsTestWaiter waiter(web_contents);
  waiter.AddWebFeatureExpectation(blink::mojom::WebFeature::kConversionAPIAll);

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      server_.GetURL(
          "a.test",
          "/attribution_reporting/page_with_impression_creator.html")));

  // Create an anchor tag with impression attributes which opens a link in a
  // new window.
  GURL link_url = server_.GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(ExecJs(web_contents, content::JsReplace(R"(
    createImpressionTag({id: 'link',
                        url: $1,
                        data: '1',
                        destination: 'https://b.test',
                        target: '_blank'});)",
                                                      link_url)));

  // Click the impression, and wait for the new window to open. Then switch to
  // the tab with the impression.
  content::WebContentsAddedObserver window_observer;
  EXPECT_TRUE(ExecJs(web_contents, "simulateClick('link');"));
  content::WebContents* new_contents = window_observer.GetWebContents();
  WaitForLoadStop(new_contents);
  browser()->tab_strip_model()->ActivateTabAt(0);
  waiter.Wait();

  // Navigate to a new page to flush metrics.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kImpressionRegistration, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kConversionAPIAll,
      1);
}

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       ConversionPing_FeatureRecorded) {
  // The test assumes the previous page gets deleted after navigation,
  // triggering histogram recording. Disable back/forward cache to ensure that
  // it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      server_.GetURL(
          "a.test",
          "/attribution_reporting/page_with_conversion_redirect.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Register a conversion with the original page as the reporting origin.
  EXPECT_TRUE(ExecJs(web_contents, "registerConversion({data: 7})"));

  // Wait for the conversion redirect to be intercepted. This is indicated by
  // window title changing when the img element for the conversion request fires
  // an onerror event.
  const std::u16string kConvertTitle = u"converted";
  content::TitleWatcher watcher(web_contents, kConvertTitle);
  EXPECT_EQ(kConvertTitle, watcher.WaitAndGetTitle());

  // Navigate to a new page to flush metrics.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kConversionRegistration, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kConversionAPIAll,
      1);
}

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       WindowOpenWithOnlyAttributionFeatures_LinkOpenedInTab) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      server_.GetURL(
          "a.test",
          "/attribution_reporting/page_with_impression_creator.html")));

  // Create an observer to catch the opened WebContents.
  content::WebContentsAddedObserver window_observer;

  GURL register_url = server_.GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  GURL link_url = server_.GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  // Navigate the page using window.open and set an attribution source.
  EXPECT_TRUE(ExecJs(web_contents, content::JsReplace(R"(
    window.open($1, "_blank", "attributionsrc="+$2);)",
                                                      link_url, register_url)));

  content::WebContents* new_contents = window_observer.GetWebContents();
  WaitForLoadStop(new_contents);

  // Ensure the window was opened in a new tab. If the window is in a new popup
  // the web contents would not belong to the tab strip.
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetIndexOfWebContents(new_contents));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
}

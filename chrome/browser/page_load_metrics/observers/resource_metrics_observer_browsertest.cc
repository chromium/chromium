// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/observers/resource_metrics_observer.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class ResourceMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  ResourceMetricsObserverBrowserTest() {}

  ~ResourceMetricsObserverBrowserTest() override {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js")});
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/ad_tagging");
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents);
  }
};

IN_PROC_BROWSER_TEST_F(ResourceMetricsObserverBrowserTest,
                       RecordUnfinishedResourceToMetrics) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ads_observer");
  content::SetupCrossSiteRedirector(embedded_test_server());

  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL(
      "foo.com", "/ad_with_incomplete_resource.html");
  ui_test_utils::NavigateToURL(browser(), url);

  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();

  incomplete_resource_response->WaitForRequest();
  incomplete_resource_response->Send(kHttpResponseHeader);
  incomplete_resource_response->Send(std::string(2048, ' '));

  // Wait for the resource update to be received for the incomplete response.
  waiter->AddMinimumNetworkBytesExpectation(2048);
  waiter->Wait();

  // Verify correct numbers of resources are recorded. A favicon is
  // automatically loaded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource", 2);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.AdResource", 1);

  // Unfinished resource not yet recorded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 0);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.VanillaResource", 0);

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControllableHttpResponse.
  browser()->tab_strip_model()->CloseAllTabs();

  // Verify unfinished resource recorded when page is destroyed.
  histogram_tester.ExpectUniqueSample(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 2, 1);
}

// Verify that per-resource metrics are reported for cached resources and
// resources loaded by the network.
IN_PROC_BROWSER_TEST_F(ResourceMetricsObserverBrowserTest,
                       RecordedCacheResourceMetrics) {
  base::HistogramTester histogram_tester;
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.com", "/cachetime"));

  // Wait for the favicon to be fetched.
  waiter->AddMinimumCompleteResourcesExpectation(2);
  waiter->Wait();

  // All resources should have been loaded by network.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource", 2);

  // Open a new tab and navigate so that resources are fetched via the disk
  // cache. Navigating to the same URL in the same tab triggers a refresh which
  // will not check the disk cache.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.com", "/cachetime"));

  // Wait for the resource to be fetched.
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->Wait();

  // Resource should be recorded as loaded from the cache. Favicon not
  // fetched this time.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Cache2.Mainframe.VanillaResource", 1);
}

// Verify that Mime type metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(ResourceMetricsObserverBrowserTest,
                       RecordedMimeMetrics) {
  base::HistogramTester histogram_tester;
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("foo.com", "/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(
      ExecJs(contents, "createAdFrame('multiple_mimes.html', 'test');"));
  waiter->AddMinimumCompleteResourcesExpectation(11);
  waiter->Wait();

  // Close all tabs to log metrics, as the video resource request is incomplete.
  browser()->tab_strip_model()->CloseAllTabs();

  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.HTML",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.CSS",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.JS",
                                    3);

  // Note: png and video/webm mime types are not set explicitly by the
  // embedded_test_server.
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Image",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Video",
                                    1);
  histogram_tester.ExpectTotalCount("Ads.ResourceUsage.Size.Network.Mime.Other",
                                    1);
}

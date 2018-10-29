// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_test_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/download/download_stats.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.Google.FrameCounts.AdFrames.PerFrame.OriginStatus";

enum class Origin {
  kNavigation,
  kAnchorAttribute,
};

using FrameType = blink::DownloadStats::FrameType;
using GestureType = blink::DownloadStats::GestureType;
using MetadataInfo = std::tuple<Origin, FrameType, GestureType>;

std::string ToString(Origin origin) {
  switch (origin) {
    case Origin::kNavigation:
      return "Navigation";
    case Origin::kAnchorAttribute:
      return "AnchorAttribute";
  }
}

std::string ToString(FrameType type) {
  switch (type) {
    case FrameType::kMainFrame:
      return "MainFrame";
    case FrameType::kSameOriginAdSubframe:
      return "SameOriginAdSubframe";
    case FrameType::kSameOriginNonAdSubframe:
      return "SameOriginNonAdSubframe";
    case FrameType::kCrossOriginAdSubframe:
      return "CrossOriginAdSubframe";
    case FrameType::kCrossOriginNonAdSubframe:
      return "CrossOriginNonAdSubframe";
  }
}

std::string ToString(GestureType gesture) {
  switch (gesture) {
    case GestureType::kWithoutGesture:
      return "Without_Gesture";
    case GestureType::kWithGesture:
      return "With_Gesture";
  }
}

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest()
      : subresource_filter::SubresourceFilterBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAdsFeature);
  }
  ~AdsPageLoadMetricsObserverBrowserTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverBrowserTest);
};

// Test that an embedded ad is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbedded) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/srcdoc_embedded_ad_empty.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricSame) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kSame, 1);
}

// Test that an ad with a different origin as the main page is cross origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricCross) {
  // Note: Cannot navigate cross-origin without dynamically generating the URL.
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  // Note that the initial iframe is not an ad, so the metric doesn't observe
  // it initially as same origin.  However, on re-navigating to a cross
  // origin site that has an ad at its origin, the ad on that page is cross
  // origin from the original page.
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "a.com", "/ads_observer/same_origin_ad.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId,
      AdsPageLoadMetricsObserver::AdOriginStatus::kCross, 1);
}

// Test that a subframe that aborts (due to doc.write) doesn't cause a crash
// if it continues to load resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DocOverwritesNavigation) {
  content::DOMMessageQueue msg_queue;

  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/ads_observer/docwrite_provisional_frame.html"));
  std::string status;
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_EQ("\"loaded\"", status);

  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total",
      0 /* < 1 KB */, 1);
}

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       SubresourceFilter) {
  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kDryRun,
      subresource_filter::ActivationScope::ALL_SITES));
  base::HistogramTester histogram_tester;

  // cross_site_iframe_factory loads URLs like:
  // http://b.com:40919/cross_site_iframe_factory.html?b()
  SetRulesetToDisallowURLsWithPathSuffix("b()");
  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c,d)"));

  ui_test_utils::NavigateToURL(browser(), main_url);
  // Navigate away to force the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.SubresourceFilter.FrameCounts.AnyParentFrame."
      "AdFrames",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Clients.Ads.All.FrameCounts.AnyParentFrame.AdFrames", 2, 1);
}

class AdsPageLoadMetricsTestWaiter
    : public page_load_metrics::PageLoadMetricsTestWaiter {
 public:
  explicit AdsPageLoadMetricsTestWaiter(content::WebContents* web_contents)
      : page_load_metrics::PageLoadMetricsTestWaiter(web_contents) {}
  void AddMinimumAdResourceExpectation(int num_ad_resources) {
    expected_minimum_num_ad_resources_ = num_ad_resources;
  }

 protected:
  bool ExpectationsSatisfied() const override {
    int num_ad_resources = 0;
    for (auto& kv : page_resources_) {
      if (kv.second->reported_as_ad_resource)
        num_ad_resources++;
    }
    return num_ad_resources >= expected_minimum_num_ad_resources_ &&
           PageLoadMetricsTestWaiter::ExpectationsSatisfied();
  };

 private:
  int expected_minimum_num_ad_resources_ = 0;
};

class AdsPageLoadMetricsObserverResourceBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<MetadataInfo> {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAdsFeature);
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override {}
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js"),
         subresource_filter::testing::CreateSuffixRule("disallow.zip")});
  }

  void OpenLinkInFrame(const content::ToRenderFrameHost& adapter,
                       const std::string& link_id,
                       GestureType gesture) {
    std::string open_link_script = base::StringPrintf(
        R"(
            var evt = document.createEvent("MouseEvent");
            evt.initMouseEvent('click', true, true);
            document.getElementById('%s').dispatchEvent(evt);
        )",
        link_id.c_str());
    if (gesture == GestureType::kWithGesture) {
      EXPECT_TRUE(ExecuteScript(adapter, open_link_script));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGesture(adapter, open_link_script));
    }
  }

 protected:
  std::unique_ptr<AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<AdsPageLoadMetricsTestWaiter>(web_contents);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResources) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

// Main resources for adframes are counted as ad resources.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedMainResourceAds) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', '');"));
  // Two pages subresources should have been reported as ad. The iframe resource
  // should also be reported as an ad.
  waiter->AddMinimumAdResourceExpectation(5);
  waiter->Wait();
}

// Subframe navigations report ad resources correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedSubframeNavigationAds) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('frame_factory.html', 'test');"));
  waiter->AddMinimumAdResourceExpectation(5);
  waiter->Wait();
  NavigateIframeToURL(
      web_contents(), "test",
      embedded_test_server()->GetURL("foo.com", "/frame_factory.html"));
  // The new subframe as well as two of its page subresources should be reported
  // as an ad.
  waiter->AddMinimumAdResourceExpectation(8);
  waiter->Wait();
}

// Verify that per-resource metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResourceMetrics) {
  base::HistogramTester histogram_tester;

  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  auto ad_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ad_script.js",
          true /*relative_url_is_prefix*/);
  auto iframe_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/iframe.html",
          true /*relative_url_is_prefix*/);
  auto vanilla_script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/vanilla_script.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/mock_page.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  main_html_response->WaitForRequest();
  main_html_response->Send(kHttpResponseHeader);
  main_html_response->Send(
      "<html><body></body><script src=\"ad_script.js\"></script></html>");
  main_html_response->Done();

  ad_script_response->WaitForRequest();
  ad_script_response->Send(kHttpResponseHeader);
  ad_script_response->Send(
      "var iframe = document.createElement(\"iframe\");"
      "iframe.src =\"iframe.html\";"
      "document.body.appendChild(iframe);");
  ad_script_response->Send(std::string(1000, ' '));
  ad_script_response->Done();

  iframe_response->WaitForRequest();
  iframe_response->Send(kHttpResponseHeader);
  iframe_response->Send("<html><script src=\"vanilla_script.js\"></script>");
  iframe_response->Send(std::string(2000, ' '));
  iframe_response->Send("</html>");
  iframe_response->Done();

  vanilla_script_response->WaitForRequest();
  vanilla_script_response->Send(kHttpResponseHeader);
  vanilla_script_response->Send(std::string(1024, ' '));
  waiter->AddMinimumResourceBytesExpectation(4000);
  waiter->Wait();

  // Verify correct numbers of resources are recorded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.VanillaResource", 1);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Mainframe.AdResource", 1);
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 1);
  // Verify unfinished resource not yet recorded.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.VanillaResource", 0);

  // Close all tabs instead of navigating as the embedded_test_server will
  // hang waiting for loads to finish when we have an unfinished
  // ControlledHttpReseonse.
  browser()->tab_strip_model()->CloseAllTabs();

  // Verify unfinished resource recorded when page is destroyed.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Network.Subframe.AdResource", 2);

  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.Total", 4, 1);
  // We have received 4 KB of ads and 1 KB of toplevel ads.
  histogram_tester.ExpectBucketCount("PageLoad.Clients.Ads.Resources.Bytes.Ads",
                                     4, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.TopLevelAds", 1, 1);

  // 4 resources loaded, one unfinished.
  histogram_tester.ExpectBucketCount(
      "PageLoad.Clients.Ads.Resources.Bytes.Unfinished", 1, 1);
}

// Verify that per-resource metrics are reported for cached resources and
// resources loaded by the network.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedCacheResourceMetrics) {
  base::HistogramTester histogram_tester;
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("create_frame.js")});
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
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
  waiter = CreateAdsPageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.com", "/cachetime"));

  // Wait for the resource to be fetched.
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->Wait();

  // Resource should be recorded as loaded from the cache. Favicon not
  // fetched this time.
  histogram_tester.ExpectTotalCount(
      "Ads.ResourceUsage.Size.Cache.Mainframe.VanillaResource", 1);
}

// Verify that Mime type metrics are recorded correctly.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       RecordedMimeMetrics) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("foo.com", "/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), url);
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createAdFrame('multiple_mimes.html', 'test');"));
  waiter->AddMinimumAdResourceExpectation(8);
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

  // Verify UKM Metrics recorded.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.front(), url);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            0);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdBytesPerSecondName),
      0);

  // TTI is not reached by this page and thus should not have this recorded.
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entries.front(),
      ukm::builders::AdPageLoad::kAdBytesPerSecondAfterInteractiveName));
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      0);
  EXPECT_GT(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            0);
}

// Download gets blocked when LoadPolicy is DISALLOW for the navigation
// to download
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       DownloadBlocked) {
  ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ALL_SITES));

  base::HistogramTester histogram_tester;

  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string host_name = "foo.com";
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(host_name, "/frame_factory.html"));
  content::TestNavigationObserver navigation_observer(web_contents());
  contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("createFrame('download.html', 'test');"));
  navigation_observer.Wait();

  content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
      web_contents(), base::BindRepeating(&content::FrameMatchesName, "test"));
  OpenLinkInFrame(rfh, "blocked_nav_download_id", GestureType::kWithoutGesture);

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount("Download.FrameGesture", 0);
}

// Download events are reported correctly.
IN_PROC_BROWSER_TEST_P(AdsPageLoadMetricsObserverResourceBrowserTest,
                       Download) {
  Origin origin;
  FrameType frame_type;
  GestureType gesture_type;
  std::tie(origin, frame_type, gesture_type) = GetParam();
  SCOPED_TRACE(::testing::Message()
               << "origin = " << ToString(origin) << ", "
               << "frame_type = " << ToString(frame_type) << ", "
               << "gesture_type = " << ToString(gesture_type));

  base::HistogramTester histogram_tester;
  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverTerminal(
          content::BrowserContext::GetDownloadManager(browser()->profile()),
          1 /* wait_count */,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/ad_tagging");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string host_name = "foo.com";
  std::string initial_url = (frame_type == FrameType::kMainFrame)
                                ? "/download.html"
                                : "/frame_factory.html";
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(host_name, initial_url));

  std::string link_id =
      origin == Origin::kNavigation ? "nav_download_id" : "anchor_download_id";

  if (frame_type == FrameType::kMainFrame) {
    OpenLinkInFrame(web_contents(), link_id, gesture_type);
  } else {
    bool same_origin = frame_type == FrameType::kSameOriginAdSubframe ||
                       frame_type == FrameType::kSameOriginNonAdSubframe;
    bool ad_subframe = frame_type == FrameType::kSameOriginAdSubframe ||
                       frame_type == FrameType::kCrossOriginAdSubframe;
    content::TestNavigationObserver navigation_observer(web_contents());
    std::string script = base::StringPrintf(
        "%s('%s','%s');", ad_subframe ? "createAdFrame" : "createFrame",
        embedded_test_server()
            ->GetURL(same_origin ? host_name : "bar.com", "/download.html")
            .spec()
            .c_str(),
        "test");
    contents->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(script));
    navigation_observer.Wait();

    content::RenderFrameHost* rfh = content::FrameMatchingPredicate(
        web_contents(),
        base::BindRepeating(&content::FrameMatchesName, "test"));
    OpenLinkInFrame(rfh, link_id, gesture_type);
  }

  download_observer->WaitForFinished();
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectUniqueSample(
      "Download.FrameGesture",
      blink::DownloadStats::GetMetricsEnum(frame_type, gesture_type),
      1 /* expected_count */);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    AdsPageLoadMetricsObserverResourceBrowserTest,
    ::testing::Combine(::testing::Values(Origin::kNavigation,
                                         Origin::kAnchorAttribute),
                       ::testing::Values(FrameType::kMainFrame,
                                         FrameType::kSameOriginAdSubframe,
                                         FrameType::kSameOriginNonAdSubframe,
                                         FrameType::kCrossOriginAdSubframe,
                                         FrameType::kCrossOriginNonAdSubframe),
                       ::testing::Values(GestureType::kWithoutGesture,
                                         GestureType::kWithGesture)));

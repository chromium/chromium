// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/prerender_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/prerender_histograms.h"
#include "components/no_state_prefetch/common/prerender_origin.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using page_load_metrics::PageLoadMetricsTestWaiter;
using TimingField = page_load_metrics::PageLoadMetricsTestWaiter::TimingField;
using WebFeature = blink::mojom::WebFeature;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using NoStatePrefetch = ukm::builders::NoStatePrefetch;

namespace {

constexpr char kCacheablePathPrefix[] = "/cacheable";

std::unique_ptr<net::test_server::HttpResponse> HandleCachableRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, kCacheablePathPrefix,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  if (request.headers.find("If-None-Match") != request.headers.end()) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        "HTTP/1.1 304 Not Modified", "");
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->AddCustomHeader("cache-control", "max-age=60");
  response->AddCustomHeader("etag", "foobar");
  response->set_content("hi");
  return std::move(response);
}

}  // namespace

class PageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadMetricsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {ukm::kUkmFeature, blink::features::kPortals,
         blink::features::kPortalsCrossOrigin},
        {});
  }

  ~PageLoadMetricsBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleCachableRequestHandler));
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Force navigation to a new page, so the currently tracked page load runs its
  // OnComplete callback. You should prefer to use PageLoadMetricsTestWaiter,
  // and only use NavigateToUntrackedUrl for cases where the waiter isn't
  // sufficient.
  void NavigateToUntrackedUrl() {
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  void MakeComponentFullscreen(const std::string& id) {
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "document.getElementById(\"" + id + "\").webkitRequestFullscreen();"));
  }

  std::string GetRecordedPageLoadMetricNames() {
    auto entries = histogram_tester_->GetTotalCountsForPrefix("PageLoad.");
    std::vector<std::string> names;
    std::transform(
        entries.begin(), entries.end(), std::back_inserter(names),
        [](const std::pair<std::string, base::HistogramBase::Count>& entry) {
          return entry.first;
        });
    return base::JoinString(names, ",");
  }

  bool NoPageLoadMetricsRecorded() {
    // Determine whether any 'public' page load metrics are recorded. We exclude
    // 'internal' metrics as these may be recorded for debugging purposes.
    size_t total_pageload_histograms =
        histogram_tester_->GetTotalCountsForPrefix("PageLoad.").size();
    size_t total_internal_histograms =
        histogram_tester_->GetTotalCountsForPrefix("PageLoad.Internal.").size();
    DCHECK_GE(total_pageload_histograms, total_internal_histograms);
    return total_pageload_histograms - total_internal_histograms == 0;
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  // Triggers nostate prefetch of |url|.
  void TriggerNoStatePrefetch(const GURL& url) {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(no_state_prefetch_manager);

    prerender::test_utils::TestNoStatePrefetchContentsFactory*
        no_state_prefetch_contents_factory =
            new prerender::test_utils::TestNoStatePrefetchContentsFactory();
    no_state_prefetch_manager->SetNoStatePrefetchContentsFactoryForTest(
        no_state_prefetch_contents_factory);

    content::SessionStorageNamespace* storage_namespace =
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .GetDefaultSessionStorageNamespace();
    ASSERT_TRUE(storage_namespace);

    std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
        no_state_prefetch_contents_factory->ExpectNoStatePrefetchContents(
            prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

    std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle =
        no_state_prefetch_manager->AddPrerenderFromOmnibox(
            url, storage_namespace, gfx::Size(640, 480));
    ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());

    // The final status may be either  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED or
    // FINAL_STATUS_RECENTLY_VISITED.
    test_prerender->contents()->set_skip_final_checks(true);
  }

  void VerifyBasicPageLoadUkms(const GURL& expected_source_url) {
    using PageLoad = ukm::builders::PageLoad;
    const auto& entries =
        test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto& kv : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(),
                                                  expected_source_url);
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::
              kDocumentTiming_NavigationToDOMContentLoadedEventFiredName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kDocumentTiming_NavigationToLoadEventFiredName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(), PageLoad::kPaintTiming_NavigationToFirstPaintName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(), PageLoad::kMainFrameResource_SocketReusedName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(), PageLoad::kMainFrameResource_DNSDelayName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(), PageLoad::kMainFrameResource_ConnectDelayName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kMainFrameResource_RequestStartToSendStartName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kMainFrameResource_SendStartToReceiveHeadersEndName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kMainFrameResource_RequestStartToReceiveHeadersEndName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kMainFrameResource_NavigationStartToRequestStartName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(),
          PageLoad::kMainFrameResource_HttpProtocolSchemeName));
      EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
          kv.second.get(), PageLoad::kSiteEngagementScoreName));
    }
  }

  void VerifyNavigationMetrics(std::vector<GURL> expected_source_urls) {
    int expected_count = expected_source_urls.size();

    // Verify if the elapsed time from the navigation start are recorded.
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramNavigationTimingNavigationStartToFirstRequestStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramNavigationTimingNavigationStartToFirstResponseStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingNavigationStartToFirstLoaderCallback,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramNavigationTimingNavigationStartToFinalRequestStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramNavigationTimingNavigationStartToFinalResponseStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingNavigationStartToFinalLoaderCallback,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingNavigationStartToNavigationCommitSent,
        expected_count);

    // Verify if the intervals between adjacent milestones are recorded.
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingFirstRequestStartToFirstResponseStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingFirstResponseStartToFirstLoaderCallback,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingFinalRequestStartToFinalResponseStart,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingFinalResponseStartToFinalLoaderCallback,
        expected_count);
    histogram_tester_->ExpectTotalCount(
        internal::
            kHistogramNavigationTimingFinalLoaderCallbackToNavigationCommitSent,
        expected_count);

    using ukm::builders::NavigationTiming;
    const std::vector<const char*> metrics = {
        NavigationTiming::kFirstRequestStartName,
        NavigationTiming::kFirstResponseStartName,
        NavigationTiming::kFirstLoaderCallbackName,
        NavigationTiming::kFinalRequestStartName,
        NavigationTiming::kFinalResponseStartName,
        NavigationTiming::kFinalLoaderCallbackName,
        NavigationTiming::kNavigationCommitSentName};

    const auto& entries = test_ukm_recorder_->GetMergedEntriesByName(
        NavigationTiming::kEntryName);
    ASSERT_EQ(expected_source_urls.size(), entries.size());
    int i = 0;
    for (const auto& kv : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(),
                                                  expected_source_urls[i++]);

      // Verify if the elapsed times from the navigation start are recorded.
      for (const char* metric : metrics) {
        EXPECT_TRUE(
            test_ukm_recorder_->EntryHasMetric(kv.second.get(), metric));
      }
    }
  }

  enum class EarlyHintsPolicy {
    // Navigation will receive no early hints response.
    NoHints,

    // Navigation will receive one early hints response.
    OneHint,

    // Navigation will receive multiple early hints responses.
    MultipleHints,
  };

  content::NavigationHandleTiming NavigateWithEarlyHints(
      EarlyHintsPolicy policy) {
    auto response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_page.html",
            /*relative_url_is_prefix=*/true);

    EXPECT_TRUE(embedded_test_server()->Start());

    GURL url = embedded_test_server()->GetURL("/mock_page.html");

    content::NavigationHandleObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(), url);

    browser()->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    response->WaitForRequest();

    // Serve the Early Hints responses.
    const char kHttpResponseHeader[] =
        "HTTP/1.1 103 Early Hints\r\n"
        "\r\n";
    switch (policy) {
      case EarlyHintsPolicy::NoHints:
        break;
      case EarlyHintsPolicy::OneHint:
        response->Send(kHttpResponseHeader);
        break;
      case EarlyHintsPolicy::MultipleHints:
        response->Send(kHttpResponseHeader);
        response->Send(kHttpResponseHeader);
        response->Send(kHttpResponseHeader);
        break;
    }

    // Serve the main response.
    const char kMainHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n";
    response->Send(kMainHttpResponseHeader);
    response->Done();

    // Wait until the navigation completes.
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddMinimumCompleteResourcesExpectation(1);
    waiter->Wait();
    EXPECT_TRUE(observer.has_committed());

    // Force navigation to another page, which should force logging of
    // histograms persisted at the end of the page load lifetime.
    NavigateToUntrackedUrl();

    VerifyNavigationMetrics({url});

    return observer.navigation_handle_timing();
  }

  content::NavigationHandleTiming RedirectWithEarlyHints(
      EarlyHintsPolicy policy_for_first_request,
      EarlyHintsPolicy policy_for_final_request) {
    auto response1 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_page1.html",
            /*relative_url_is_prefix=*/true);
    auto response2 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_page2.html",
            /*relative_url_is_prefix=*/true);

    EXPECT_TRUE(embedded_test_server()->Start());

    GURL url1 = embedded_test_server()->GetURL("/mock_page1.html");
    GURL url2 = embedded_test_server()->GetURL("/mock_page2.html");

    content::NavigationHandleObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(), url1);

    browser()->OpenURL(content::OpenURLParams(
        url1, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false));
    response1->WaitForRequest();

    // Serve the Early Hints responses for the first request.
    const char kHttpResponseHeader[] =
        "HTTP/1.1 103 Early Hints\r\n"
        "\r\n";
    switch (policy_for_first_request) {
      case EarlyHintsPolicy::NoHints:
        break;
      case EarlyHintsPolicy::OneHint:
        response1->Send(kHttpResponseHeader);
        break;
      case EarlyHintsPolicy::MultipleHints:
        response1->Send(kHttpResponseHeader);
        response1->Send(kHttpResponseHeader);
        response1->Send(kHttpResponseHeader);
        break;
    }

    // Serve the redirection response.
    const char kHttpRedirectResponseHeader[] =
        "HTTP/1.1 307 Temporary Redirect\r\n"
        "Location: /mock_page2.html\r\n"
        "\r\n";
    response1->Send(kHttpRedirectResponseHeader);

    // Wait for the redirected request.
    response1->Done();
    response2->WaitForRequest();

    // Serve the Early Hints responses for the redirected request.
    switch (policy_for_final_request) {
      case EarlyHintsPolicy::NoHints:
        break;
      case EarlyHintsPolicy::OneHint:
        response2->Send(kHttpResponseHeader);
        break;
      case EarlyHintsPolicy::MultipleHints:
        response2->Send(kHttpResponseHeader);
        response2->Send(kHttpResponseHeader);
        response2->Send(kHttpResponseHeader);
        break;
    }

    // Serve the main response.
    const char kMainHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n";
    response2->Send(kMainHttpResponseHeader);
    response2->Done();

    // Wait until the navigation completes.
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddMinimumCompleteResourcesExpectation(1);
    waiter->Wait();
    EXPECT_TRUE(observer.has_committed());

    // Force navigation to another page, which should force logging of
    // histograms persisted at the end of the page load lifetime.
    NavigateToUntrackedUrl();

    VerifyNavigationMetrics({url2});

    return observer.navigation_handle_timing();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetMainFrame();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramParseDuration, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_->ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  VerifyBasicPageLoadUkms(url);

  const auto& nostate_prefetch_entries =
      test_ukm_recorder_->GetMergedEntriesByName(NoStatePrefetch::kEntryName);
  EXPECT_EQ(0u, nostate_prefetch_entries.size());

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, Redirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL("/title1.html");
  GURL first_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), first_url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramParseDuration, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_->ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  VerifyBasicPageLoadUkms(final_url);

  const auto& nostate_prefetch_entries =
      test_ukm_recorder_->GetMergedEntriesByName(NoStatePrefetch::kEntryName);
  EXPECT_EQ(0u, nostate_prefetch_entries.size());

  VerifyNavigationMetrics({final_url});
}

// Triggers nostate prefetch, and verifies that the UKM metrics related to
// nostate prefetch are recorded correctly.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoStatePrefetchMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");

  TriggerNoStatePrefetch(url);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_->ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NoStatePrefetch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(), url);
    // UKM metrics related to attempted nostate prefetch should be recorded.
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NoStatePrefetch::kPrefetchedRecently_FinalStatusName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NoStatePrefetch::kPrefetchedRecently_OriginName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NoStatePrefetch::kPrefetchedRecently_PrefetchAgeName));
  }

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, CachedPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL(kCacheablePathPrefix);

  // Navigate to the |url| to cache the main resource.
  ui_test_utils::NavigateToURL(browser(), url);
  NavigateToUntrackedUrl();

  using PageLoad = ukm::builders::PageLoad;
  auto entries =
      test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    auto* const uncached_load_entry = kv.second.get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(uncached_load_entry, url);

    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(uncached_load_entry,
                                                    PageLoad::kWasCachedName));
  }

  VerifyNavigationMetrics({url});

  // Reset the recorders so it would only contain the cached pageload.
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  // Second navigation to the |url| should hit cache.
  ui_test_utils::NavigateToURL(browser(), url);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  entries = test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    auto* const cached_load_entry = kv.second.get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(cached_load_entry, url);

    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(cached_load_entry,
                                                   PageLoad::kWasCachedName));
  }

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPageInNewForegroundTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateParams params(browser(),
                        embedded_test_server()->GetURL("/title1.html"),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  Navigate(&params);
  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(
      params.navigated_or_inserted_contents);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->Wait();

  // Due to crbug.com/725347, with browser side navigation enabled, navigations
  // in new tabs were recorded as starting in the background. Here we verify
  // that navigations initiated in a new tab are recorded as happening in the
  // foreground.
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPaintForEmptyDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      0);
}

// TODO(crbug.com/986642): Flaky on Win and Linux.
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_NoPaintForEmptyDocumentInChildFrame \
  DISABLED_NoPaintForEmptyDocumentInChildFrame
#else
#define MAYBE_NoPaintForEmptyDocumentInChildFrame \
  NoPaintForEmptyDocumentInChildFrame
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_NoPaintForEmptyDocumentInChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(
      embedded_test_server()->GetURL("/page_load_metrics/empty_iframe.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddSubFrameExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframe.html"));
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInDynamicChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/dynamic_iframe.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMultipleChildFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframes.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);

  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMainAndChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL(
      "/page_load_metrics/main_frame_with_iframe.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  // Perform a same-document navigation. No additional metrics should be logged.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html#hash"));
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameUrlNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();

  VerifyNavigationMetrics({url});

  // We expect one histogram sample for each navigation to title1.html.
  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 2);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 2);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  VerifyNavigationMetrics({url, url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       DocWriteAbortsSubframeNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/doc_write_aborts_subframe.html"));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NonHtmlMainResource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/circle.svg"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NonHttpOrHttpsUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, HttpErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/page_load_metrics/404.html"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, ChromeErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  // By shutting down the server, we ensure a failure.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  content::NavigationHandleObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), url);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(observer.is_error());
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, Ignore204Pages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/page204.html"));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, IgnoreDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,  // == wait_count (only waiting for "download-test3.gif").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/download-test3.gif"));
  downloads_observer.WaitForFinished();

  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteBlock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      1);

  // Reload should not log the histogram as the script is not blocked.
  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kDocumentWriteBlockReload);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 1);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kDocumentWriteBlockReload);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteAsync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_async_script.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWriteScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      0);
}

// TODO(crbug.com/712935): Flaky on Linux dbg.
// TODO(crbug.com/738235): Now flaky on Win and Mac.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DISABLED_BadXhtml) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // When an XHTML page contains invalid XML, it causes a paint of the error
  // message without a layout. Page load metrics currently treats this as an
  // error. Eventually, we'll fix this by special casing the handling of
  // documents with non-well-formed XML on the blink side. See crbug.com/627607
  // for more.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/badxml.xhtml"));
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 0);

  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kErrorEvents,
      page_load_metrics::ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);

  histogram_tester_->ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_ORDER_PARSE_START_FIRST_PAINT, 1);
}

// Test code that aborts provisional navigations.
// TODO(csharrison): Move these to unit tests once the navigation API in content
// properly calls NavigationHandle/NavigationThrottle methods.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortNewNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  NavigateParams params2(browser(), url2, ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  Navigate(&params2);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  NavigateParams params2(browser(), url, ui::PAGE_TRANSITION_RELOAD);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  Navigate(&params2);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramAbortReloadBeforeCommit, 1);
}

// TODO(crbug.com/675061): Flaky on Win7 dbg.
#if defined(OS_WIN)
#define MAYBE_AbortClose DISABLED_AbortClose
#else
#define MAYBE_AbortClose AbortClose
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_AbortClose) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  manager.WaitForNavigationFinished();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramAbortCloseBeforeCommit, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, AbortMultiple) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), url);

  Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  NavigateParams params2(browser(), url2, ui::PAGE_TRANSITION_TYPED);
  content::TestNavigationManager manager2(
      browser()->tab_strip_model()->GetActiveWebContents(), url2);
  Navigate(&params2);

  EXPECT_TRUE(manager2.WaitForRequestStart());
  manager.WaitForNavigationFinished();

  GURL url3(embedded_test_server()->GetURL("/title3.html"));
  NavigateParams params3(browser(), url3, ui::PAGE_TRANSITION_TYPED);

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  Navigate(&params3);
  waiter->Wait();

  manager2.WaitForNavigationFinished();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramAbortNewNavigationBeforeCommit, 2);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoAbortMetricsOnClientRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), first_url);

  GURL second_url(embedded_test_server()->GetURL("/title2.html"));
  NavigateParams params(browser(), second_url, ui::PAGE_TRANSITION_LINK);
  content::TestNavigationManager manager(
      browser()->tab_strip_model()->GetActiveWebContents(), second_url);
  Navigate(&params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  {
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(TimingField::kLoadEvent);
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.location.reload();"));
    waiter->Wait();
  }

  manager.WaitForNavigationFinished();

  EXPECT_TRUE(
      histogram_tester_
          ->GetTotalCountsForPrefix("PageLoad.Experimental.AbortTiming.")
          .empty());
}

// TODO(crbug.com/1009885): Flaky on Linux MSan builds.
#if defined(MEMORY_SANITIZER) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
#define MAYBE_FirstMeaningfulPaintRecorded DISABLED_FirstMeaningfulPaintRecorded
#else
#define MAYBE_FirstMeaningfulPaintRecorded FirstMeaningfulPaintRecorded
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_FirstMeaningfulPaintRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstMeaningfulPaint);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_->ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstMeaningfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       FirstMeaningfulPaintNotRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/page_with_active_connections.html"));
  waiter->Wait();

  // Navigate away before a FMP is reported.
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstMeaningfulPaint,
                                      0);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/large.html"));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the main HTML response).
  histogram_tester_->ExpectBucketCount(internal::kHistogramPageLoadTotalBytes,
                                       10, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSizeChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/large_iframe.html"));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the iframe response).
  histogram_tester_->ExpectBucketCount(internal::kHistogramPageLoadTotalBytes,
                                       10, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       PayloadSizeIgnoresDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,  // == wait_count (only waiting for "download-test1.lib").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/download_anchor_click.html"));
  downloads_observer.WaitForFinished();

  NavigateToUntrackedUrl();

  histogram_tester_->ExpectUniqueSample(internal::kHistogramPageLoadTotalBytes,
                                        0, 1);
}

// Test UseCounter Features observed in the main frame are recorded, exactly
// once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(
          WebFeature::kApplicationCacheManifestSelectSecureOrigin),
      1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(
          WebFeature::kApplicationCacheManifestSelectSecureOrigin),
      1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                       1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                       1);
  histogram_tester_->ExpectBucketCount(
      internal::kCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

class PageLoadMetricsBrowserTestWithAutoupgradesDisabled
    : public PageLoadMetricsBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithAutoupgradesDisabled,
                       UseCounterFeaturesMixedContent) {
  // UseCounterFeaturesInMainFrame loads the test file on a loopback
  // address. Loopback is treated as a secure origin in most ways, but it
  // doesn't count as mixed content when it loads http://
  // subresources. Therefore, this test loads the test file on a real HTTPS
  // server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentAudio), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentImage), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentVideo), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithAutoupgradesDisabled,
                       UseCounterCSSPropertiesMixedContent) {
  // UseCounterCSSPropertiesInMainFrame loads the test file on a loopback
  // address. Loopback is treated as a secure origin in most ways, but it
  // doesn't count as mixed content when it loads http://
  // subresources. Therefore, this test loads the test file on a real HTTPS
  // server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                       1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                       1);
  histogram_tester_->ExpectBucketCount(
      internal::kCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithAutoupgradesDisabled,
                       UseCounterAnimatedCSSPropertiesMixedContent) {
  // UseCounterCSSPropertiesInMainFrame loads the test file on a loopback
  // address. Loopback is treated as a secure origin in most ways, but it
  // doesn't count as mixed content when it loads http://
  // subresources. Therefore, this test loads the test file on a real HTTPS
  // server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInNonSecureMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "non-secure.test", "/page_load_metrics/use_counter_features.html"));
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kFullscreenInsecureOrigin), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kFullscreenInsecureOrigin), 1);
}

// Test UseCounter UKM features observed.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterUkmFeaturesLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL(
      "/page_load_metrics/use_counter_features.html");
  ui_test_utils::NavigateToURL(browser(), url);
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_THAT(entries, SizeIs(4));
  std::vector<int64_t> ukm_features;
  for (const auto* entry : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
    test_ukm_recorder_->ExpectEntryMetric(
        entry, ukm::builders::Blink_UseCounter::kIsMainFrameFeatureName, 1);
    const auto* metric = test_ukm_recorder_->GetEntryMetric(
        entry, ukm::builders::Blink_UseCounter::kFeatureName);
    DCHECK(metric);
    ukm_features.push_back(*metric);
  }
  EXPECT_THAT(
      ukm_features,
      UnorderedElementsAre(
          static_cast<int64_t>(WebFeature::kPageVisits),
          static_cast<int64_t>(WebFeature::kFullscreenSecureOrigin),
          static_cast<int64_t>(WebFeature::kNavigatorVibrate),
          static_cast<int64_t>(
              WebFeature::kApplicationCacheManifestSelectSecureOrigin)));
}

// Test UseCounter UKM mixed content features observed.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithAutoupgradesDisabled,
                       UseCounterUkmMixedContentFeaturesLogged) {
  // As with UseCounterFeaturesMixedContent, load on a real HTTPS server to
  // trigger mixed content.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url =
      https_server.GetURL("/page_load_metrics/use_counter_features.html");
  ui_test_utils::NavigateToURL(browser(), url);
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_THAT(entries, SizeIs(7));
  std::vector<int64_t> ukm_features;
  for (const auto* entry : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
    test_ukm_recorder_->ExpectEntryMetric(
        entry, ukm::builders::Blink_UseCounter::kIsMainFrameFeatureName, 1);
    const auto* metric = test_ukm_recorder_->GetEntryMetric(
        entry, ukm::builders::Blink_UseCounter::kFeatureName);
    DCHECK(metric);
    ukm_features.push_back(*metric);
  }
  EXPECT_THAT(ukm_features,
              UnorderedElementsAre(
                  static_cast<int64_t>(WebFeature::kPageVisits),
                  static_cast<int64_t>(WebFeature::kFullscreenSecureOrigin),
                  static_cast<int64_t>(WebFeature::kNavigatorVibrate),
                  static_cast<int64_t>(
                      WebFeature::kApplicationCacheManifestSelectSecureOrigin),
                  static_cast<int64_t>(WebFeature::kMixedContentImage),
                  static_cast<int64_t>(WebFeature::kMixedContentAudio),
                  static_cast<int64_t>(WebFeature::kMixedContentVideo)));
}

// Test UseCounter Features observed in a child frame are recorded, exactly
// once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, UseCounterFeaturesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features_in_iframe.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

// Test UseCounter Features observed in multiple child frames are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

// Test UseCounter CSS properties observed in a child frame are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features_in_iframe.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                       1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                       1);
  histogram_tester_->ExpectBucketCount(
      internal::kCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS Properties observed in multiple child frames are
// recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                       1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                       1);
  histogram_tester_->ExpectBucketCount(
      internal::kCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS properties observed in a child frame are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features_in_iframe.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS Properties observed in multiple child frames are
// recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html"));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName,
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter Features observed for SVG pages.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterObserveSVGImagePage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/circle.svg"));
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, LoadingMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadTimingInfo);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // Waits until nonzero loading metrics are seen.
  waiter->Wait();
}

class SessionRestorePageLoadMetricsBrowserTest
    : public PageLoadMetricsBrowserTest {
 public:
  SessionRestorePageLoadMetricsBrowserTest() {}

  // PageLoadMetricsBrowserTest:
  void SetUpOnMainThread() override {
    PageLoadMetricsBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* QuitBrowserAndRestore(Browser* browser) {
    Profile* profile = browser->profile();

    SessionStartupPref::SetStartupPref(
        profile, SessionStartupPref(SessionStartupPref::LAST));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);
#endif

    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);
    SessionRestoreTestHelper().Wait();
    return BrowserList::GetInstance()->GetLastActive();
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      ASSERT_TRUE(content::WaitForLoadStop(contents));
    }
  }

  // The PageLoadMetricsTestWaiter can observe first meaningful paints on these
  // test pages while not on other simple pages such as /title1.html.
  GURL GetTestURL() const {
    return embedded_test_server()->GetURL(
        "/page_load_metrics/main_frame_with_iframe.html");
  }

  GURL GetTestURL2() const {
    return embedded_test_server()->GetURL("/title2.html");
  }

  void ExpectFirstPaintMetricsTotalCount(int expected_total_count) const {
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstPaint,
        expected_total_count);
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstContentfulPaint,
        expected_total_count);
    histogram_tester_->ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstMeaningfulPaint,
        expected_total_count);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestorePageLoadMetricsBrowserTest);
};

class SessionRestorePaintWaiter : public SessionRestoreObserver {
 public:
  SessionRestorePaintWaiter() { SessionRestore::AddObserver(this); }
  ~SessionRestorePaintWaiter() { SessionRestore::RemoveObserver(this); }

  // SessionRestoreObserver implementation:
  void OnWillRestoreTab(content::WebContents* contents) override {
    chrome::InitializePageLoadMetricsForWebContents(contents);
    auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(contents);
    waiter->AddPageExpectation(TimingField::kFirstPaint);
    waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
    waiter->AddPageExpectation(TimingField::kFirstMeaningfulPaint);
    waiters_[contents] = std::move(waiter);
  }

  // First meaningful paints occur only on foreground tabs.
  void WaitForForegroundTabs(size_t num_expected_foreground_tabs) {
    size_t num_actual_foreground_tabs = 0;
    for (auto iter = waiters_.begin(); iter != waiters_.end(); ++iter) {
      if (iter->first->GetVisibility() == content::Visibility::HIDDEN)
        continue;
      iter->second->Wait();
      ++num_actual_foreground_tabs;
    }
    EXPECT_EQ(num_expected_foreground_tabs, num_actual_foreground_tabs);
  }

 private:
  std::unordered_map<content::WebContents*,
                     std::unique_ptr<PageLoadMetricsTestWaiter>>
      waiters_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestorePaintWaiter);
};

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfSingleRestoredTab) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  histogram_tester_->ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 1);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  histogram_tester_->ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfMultipleRestoredTabs) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  histogram_tester_->ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, false, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());

  histogram_tester_->ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 4);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, false, 2);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       NoSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ExpectFirstPaintMetricsTotalCount(0);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       SingleTabSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());

  SessionRestorePaintWaiter session_restore_paint_waiter;
  QuitBrowserAndRestore(browser());

  session_restore_paint_waiter.WaitForForegroundTabs(1);
  ExpectFirstPaintMetricsTotalCount(1);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       MultipleTabsSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  SessionRestorePaintWaiter session_restore_paint_waiter;
  Browser* new_browser = QuitBrowserAndRestore(browser());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());

  // Only metrics of the initial foreground tab are recorded.
  session_restore_paint_waiter.WaitForForegroundTabs(1);
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));
  ExpectFirstPaintMetricsTotalCount(1);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       NavigationDuringSessionRestore) {
  NavigateToUntrackedUrl();
  Browser* new_browser = QuitBrowserAndRestore(browser());

  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(
      new_browser->tab_strip_model()->GetActiveWebContents());
  waiter->AddPageExpectation(TimingField::kFirstMeaningfulPaint);
  ui_test_utils::NavigateToURL(new_browser, GetTestURL());
  waiter->Wait();

  // No metrics recorded for the second navigation because the tab navigated
  // away during session restore.
  ExpectFirstPaintMetricsTotalCount(0);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       LoadingAfterSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());

  Browser* new_browser = nullptr;
  {
    SessionRestorePaintWaiter session_restore_paint_waiter;
    new_browser = QuitBrowserAndRestore(browser());

    session_restore_paint_waiter.WaitForForegroundTabs(1);
    ExpectFirstPaintMetricsTotalCount(1);
  }

  // Load a new page after session restore.
  auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(
      new_browser->tab_strip_model()->GetActiveWebContents());
  waiter->AddPageExpectation(TimingField::kFirstMeaningfulPaint);
  ui_test_utils::NavigateToURL(new_browser, GetTestURL());
  waiter->Wait();

  // No more metrics because the navigation is after session restore.
  ExpectFirstPaintMetricsTotalCount(1);
}

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_InitialForegroundTabChanged DISABLED_InitialForegroundTabChanged
#else
#define MAYBE_InitialForegroundTabChanged InitialForegroundTabChanged
#endif
IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       MAYBE_InitialForegroundTabChanged) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  SessionRestorePaintWaiter session_restore_paint_waiter;
  Browser* new_browser = QuitBrowserAndRestore(browser());

  // Change the foreground tab before the first meaningful paint.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());
  tab_strip->ActivateTabAt(1, {TabStripModel::GestureType::kOther});

  session_restore_paint_waiter.WaitForForegroundTabs(1);

  // No metrics were recorded because initial foreground tab was switched away.
  ExpectFirstPaintMetricsTotalCount(0);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       MultipleSessionRestores) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());

  Browser* current_browser = browser();
  const int num_session_restores = 3;
  for (int i = 1; i <= num_session_restores; ++i) {
    SessionRestorePaintWaiter session_restore_paint_waiter;
    current_browser = QuitBrowserAndRestore(current_browser);
    session_restore_paint_waiter.WaitForForegroundTabs(1);
    ExpectFirstPaintMetricsTotalCount(i);
  }
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       RestoreForeignTab) {
  sessions::SessionTab tab;
  tab.tab_visual_index = 0;
  tab.current_navigation_index = 1;
  tab.navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
      GetTestURL().spec(), "one"));
  tab.navigations.back().set_encoded_page_state("");

  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore in the current tab.
  content::WebContents* tab_contents = nullptr;
  {
    SessionRestorePaintWaiter session_restore_paint_waiter;
    tab_contents = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::CURRENT_TAB);
    ASSERT_EQ(1, browser()->tab_strip_model()->count());
    ASSERT_TRUE(tab_contents);
    ASSERT_EQ(GetTestURL(), tab_contents->GetURL());

    session_restore_paint_waiter.WaitForForegroundTabs(1);
    ExpectFirstPaintMetricsTotalCount(1);
  }

  // Restore in a new foreground tab.
  {
    SessionRestorePaintWaiter session_restore_paint_waiter;
    tab_contents = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB);
    ASSERT_EQ(2, browser()->tab_strip_model()->count());
    ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
    ASSERT_TRUE(tab_contents);
    ASSERT_EQ(GetTestURL(), tab_contents->GetURL());

    session_restore_paint_waiter.WaitForForegroundTabs(1);
    ExpectFirstPaintMetricsTotalCount(2);
  }

  // Restore in a new background tab.
  {
    tab_contents = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::NEW_BACKGROUND_TAB);
    ASSERT_EQ(3, browser()->tab_strip_model()->count());
    ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
    ASSERT_TRUE(tab_contents);
    ASSERT_EQ(GetTestURL(), tab_contents->GetURL());
    ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(browser()));

    // Do not record timings of initially background tabs.
    ExpectFirstPaintMetricsTotalCount(2);
  }
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       RestoreForeignSession) {
  Profile* profile = browser()->profile();

  // Set up the restore data: one window with two tabs.
  std::vector<const sessions::SessionWindow*> session;
  sessions::SessionWindow window;
  {
    auto tab1 = std::make_unique<sessions::SessionTab>();
    tab1->tab_visual_index = 0;
    tab1->current_navigation_index = 0;
    tab1->pinned = true;
    tab1->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
        GetTestURL().spec(), "one"));
    tab1->navigations.back().set_encoded_page_state("");
    window.tabs.push_back(std::move(tab1));
  }

  {
    auto tab2 = std::make_unique<sessions::SessionTab>();
    tab2->tab_visual_index = 1;
    tab2->current_navigation_index = 0;
    tab2->pinned = false;
    tab2->navigations.push_back(sessions::ContentTestHelper::CreateNavigation(
        GetTestURL2().spec(), "two"));
    tab2->navigations.back().set_encoded_page_state("");
    window.tabs.push_back(std::move(tab2));
  }

  // Restore the session window with 2 tabs.
  session.push_back(static_cast<const sessions::SessionWindow*>(&window));
  SessionRestorePaintWaiter session_restore_paint_waiter;
  SessionRestore::RestoreForeignSessionWindows(profile, session.begin(),
                                               session.end());
  session_restore_paint_waiter.WaitForForegroundTabs(1);

  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));
  ExpectFirstPaintMetricsTotalCount(1);
}

// TODO(crbug.com/882077) Disabled due to flaky timeouts on all platforms.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       DISABLED_ReceivedAggregateResourceDataLength) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/cross_site_iframe_factory.html?foo"));
  waiter->Wait();
  int64_t one_frame_page_size = waiter->current_network_bytes();

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/cross_site_iframe_factory.html?a(b,c,d(e,f,g))"));
  // Verify that 7 iframes are fetched, with some amount of tolerance since
  // favicon is fetched only once.
  waiter->AddMinimumNetworkBytesExpectation(7 * (one_frame_page_size - 100));
  waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       ChunkedResponse_OverheadDoesNotCountForBodyBytes) {
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n";
  const int kChunkSize = 5;
  const int kNumChunks = 5;
  auto main_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/mock_page.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  main_response->WaitForRequest();
  main_response->Send(kHttpResponseHeader);
  for (int i = 0; i < kNumChunks; i++) {
    main_response->Send(std::to_string(kChunkSize));
    main_response->Send("\r\n");
    main_response->Send(std::string(kChunkSize, '*'));
    main_response->Send("\r\n");
  }
  main_response->Done();
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->Wait();

  // Verify that overheads for each chunk are not reported as body bytes.
  EXPECT_EQ(waiter->current_network_body_bytes(), kChunkSize * kNumChunks);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, ReceivedCompleteResources) {
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          true /*relative_url_is_prefix*/);
  auto script_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/script.js",
          true /*relative_url_is_prefix*/);
  auto iframe_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/iframe.html",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();

  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/mock_page.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  main_html_response->WaitForRequest();
  main_html_response->Send(kHttpResponseHeader);
  main_html_response->Send(
      "<html><body></body><script src=\"script.js\"></script></html>");
  main_html_response->Send(std::string(1000, ' '));
  main_html_response->Done();
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->AddMinimumNetworkBytesExpectation(1000);
  waiter->Wait();

  script_response->WaitForRequest();
  script_response->Send(kHttpResponseHeader);
  script_response->Send(
      "var iframe = document.createElement(\"iframe\");"
      "iframe.src =\"iframe.html\";"
      "document.body.appendChild(iframe);");
  script_response->Send(std::string(1000, ' '));
  // Data received but resource not complete
  waiter->AddMinimumCompleteResourcesExpectation(1);
  waiter->AddMinimumNetworkBytesExpectation(2000);
  waiter->Wait();
  script_response->Done();
  waiter->AddMinimumCompleteResourcesExpectation(2);
  waiter->Wait();

  // Make sure main resources are loaded correctly
  iframe_response->WaitForRequest();
  iframe_response->Send(kHttpResponseHeader);
  iframe_response->Send(std::string(2000, ' '));
  iframe_response->Done();
  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->AddMinimumNetworkBytesExpectation(4000);
  waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MemoryCacheResource_Recorded) {
  const char kHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Cache-Control: max-age=60\r\n"
      "\r\n";
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  auto cached_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/cachetime",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/page_with_cached_subresource.html"),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));

  // Load a resource large enough to record a nonzero number of kilobytes.
  cached_response->WaitForRequest();
  cached_response->Send(kHttpResponseHeader);
  cached_response->Send(std::string(10 * 1024, ' '));
  cached_response->Done();

  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();

  // Re-navigate the page to the same url with a different query string so the
  // main resource is not loaded from the disk cache. The subresource will be
  // loaded from the memory cache.
  waiter = CreatePageLoadMetricsTestWaiter();
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/page_with_cached_subresource.html?xyz"),
      content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));

  // Favicon is not fetched this time.
  waiter->AddMinimumCompleteResourcesExpectation(2);
  waiter->Wait();

  // Verify no resources were cached for the first load.
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 0, 1);
  histogram_tester_->ExpectBucketCount(internal::kHistogramPageLoadCacheBytes,
                                       0, 1);

  // Force histograms to record.
  NavigateToUntrackedUrl();

  // Verify that the cached resource from the memory cache is recorded
  // correctly.
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 1, 1);
  histogram_tester_->ExpectBucketCount(internal::kHistogramPageLoadCacheBytes,
                                       10, 1);
}

// Verifies that css image resources shared across document do not cause a
// crash, and are only counted once per context. https://crbug.com/979459.
// TODO(crbug.com/1108534): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       DISABLED_MemoryCacheResources_RecordedOncePerContext) {
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/document_with_css_image_sharing.html"));

  waiter->AddMinimumCompleteResourcesExpectation(7);
  waiter->Wait();

  // Force histograms to record.
  NavigateToUntrackedUrl();

  // Verify that cached resources are only reported once per context.
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 2, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, InputEventsForClick) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL("/page_load_metrics/link.html");
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  // Navigation should record the metrics twice because of the initial pageload
  // and the second pageload ("/title1.html") initiated by the link click.
  VerifyNavigationMetrics(
      {url, embedded_test_server()->GetURL("/title1.html")});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, InputEventsForOmniboxMatch) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::SendToOmniboxAndSubmit(browser(), url.spec(),
                                        base::TimeTicks::Now());
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       InputEventsForJavaScriptHref) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url =
      embedded_test_server()->GetURL("/page_load_metrics/javascript_href.html");
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();
  waiter = CreatePageLoadMetricsTestWaiter();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  // Navigation should record the metrics twice because of the initial pageload
  // and the second pageload ("/title1.html") initiated by the link click.
  VerifyNavigationMetrics(
      {url, embedded_test_server()->GetURL("/title1.html")});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       InputEventsForJavaScriptWindowOpen) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL(
      "/page_load_metrics/javascript_window_open.html");
  ui_test_utils::NavigateToURL(browser(), url);
  waiter->Wait();
  content::WebContentsAddedObserver web_contents_added_observer;
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  // Wait for new window to open.
  auto* web_contents = web_contents_added_observer.GetWebContents();
  waiter = std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);

  // Close all pages, which should force logging of histograms persisted at the
  // end of the page load lifetime.
  browser()->tab_strip_model()->CloseAllTabs();

  // Navigation should record the metrics twice because of the initial pageload
  // and the second pageload ("/title1.html") initiated by the link click.
  VerifyNavigationMetrics(
      {url, embedded_test_server()->GetURL("/title1.html")});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, FirstInputFromScroll) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/scroll.html"));
  waiter->Wait();

  content::SimulateGestureScrollSequence(
      browser()->tab_strip_model()->GetActiveWebContents(),
      gfx::Point(100, 100), gfx::Vector2dF(0, 15));
  NavigateToUntrackedUrl();

  // First Input Delay should not be reported from a scroll!
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputTimestamp,
                                      0);
}

// Does a navigation to a page controlled by a service worker and verifies
// that service worker page load metrics are logged.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, ServiceWorkerMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a page that registers a service worker.
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('fetch_event_pass_through.js');"));
  waiter->Wait();

  // The first load was not controlled, so service worker metrics should not be
  // logged.
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 0);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a controlled page.
  GURL controlled_url = url;
  ui_test_utils::NavigateToURL(browser(), controlled_url);
  waiter->Wait();

  // Service worker metrics should be logged.
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 2);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  // Navigation should record the metrics twice because of the initial pageload
  // to register a service worker and the page load controlled by the service
  // worker.
  VerifyNavigationMetrics({url, controlled_url});
}

// Does a navigation to a page which records a WebFeature before commit.
// Regression test for https://crbug.com/1043018.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PreCommitWebFeature) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kSecureContextCheckPassed), 1);
  histogram_tester_->ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kSecureContextCheckFailed), 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionsMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Evaluate the height and width of the page as the browser_test can
  // vary the dimensions.
  int document_height =
      EvalJs(web_contents, "document.body.scrollHeight").ExtractInt();
  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  // Expectation is before NavigateToUrl for this test as the expectation can be
  // met after NavigateToUrl and before the Wait.
  waiter->AddMainFrameIntersectionExpectation(
      gfx::Rect(0, 0, document_width,
                document_height));  // Initial main frame rect.

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  waiter->Wait();

  // Create a |document_width|x|document_height| frame at 100,100, increasing
  // the page width and height by 100.
  waiter->AddMainFrameIntersectionExpectation(
      gfx::Rect(0, 0, document_width + 100, document_height + 100));
  EXPECT_TRUE(ExecJs(
      web_contents,
      content::JsReplace("createIframeAtRect(\"test\", 100, 100, $1, $2);",
                         document_width, document_height)));
  waiter->Wait();
}

// Creates a single frame within the main frame and verifies the intersection
// with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionSingleFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));

  waiter->Wait();
}

// Creates a set of nested frames within the main frame and verifies
// their intersections with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionSameOrigin) {
  EXPECT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));
  waiter->Wait();

  NavigateIframeToURL(
      web_contents, "test",
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));

  // Creates the grandchild iframe within the child frame at 10, 10 with
  // dimensions 300x300. This frame is clipped by 110 pixels in the bottom and
  // right. This translates to an intersection of 110, 110, 190, 190 with the
  // main frame.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(110, 110, 190, 190));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_TRUE(
      ExecJs(child_frame, "createIframeAtRect(\"test2\", 10, 10, 300, 300);"));

  waiter->Wait();
}

// Creates a set of nested frames, with a cross origin subframe, within the
// main frame and verifies their intersections with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionCrossOrigin) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));

  NavigateIframeToURL(
      web_contents, "test",
      embedded_test_server()->GetURL(
          "b.com",
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));

  // Wait for the main frame intersection after we have navigated the frame
  // to a cross-origin url.
  waiter->Wait();

  // Change the size of the frame to 150, 150. This tests the cross origin
  // code path as the previous wait can flakily pass due to receiving the
  // correct intersection before the frame transitions to cross-origin without
  // checking that the final computation is consistent.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 150, 150));
  EXPECT_TRUE(ExecJs(web_contents,
                     "let frame = document.getElementById('test'); "
                     "frame.width = 150; "
                     "frame.height = 150; "));
  waiter->Wait();

  // Creates the grandchild iframe within the child frame at 10, 10 with
  // dimensions 300x300. This frame is clipped by 110 pixels in the bottom and
  // right. This translates to an intersection of 110, 110, 190, 190 with the
  // main frame.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(110, 110, 140, 140));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_TRUE(
      ExecJs(child_frame, "createIframeAtRect(\"test2\", 10, 10, 300, 300);"));

  waiter->Wait();
}

// Creates a set of nested frames, with a cross origin subframe that is out of
// view within the main frame and verifies their intersections with the main
// frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionCrossOriginOutOfView) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));

  NavigateIframeToURL(
      web_contents, "test",
      embedded_test_server()->GetURL(
          "b.com",
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));

  // Wait for the main frame intersection after we have navigated the frame
  // to a cross-origin url.
  waiter->Wait();

  // Creates the grandchild iframe within the child frame outside the parent
  // frame's viewport.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(0, 0, 0, 0));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(child_frame,
                     "createIframeAtRect(\"test2\", 5000, 5000, 190, 190);"));

  waiter->Wait();
}

// Creates a set of nested frames, with a cross origin subframe that is out of
// view within the main frame and verifies their intersections with the main
// frame. The out of view frame is then scrolled back into view and the
// intersection is verified.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameIntersectionCrossOriginScrolled) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));

  NavigateIframeToURL(
      web_contents, "test",
      embedded_test_server()->GetURL(
          "b.com",
          "/page_load_metrics/blank_with_positioned_iframe_writer.html"));

  // Wait for the main frame intersection after we have navigated the frame
  // to a cross-origin url.
  waiter->Wait();

  // Creates the grandchild iframe within the child frame outside the parent
  // frame's viewport.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(0, 0, 0, 0));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  EXPECT_TRUE(ExecJs(child_frame,
                     "createIframeAtRect(\"test2\", 5000, 5000, 190, 190);"));
  waiter->Wait();

  // Scroll the child frame and verify the grandchild frame's intersection.
  // The parent frame is at position 100,100 with dimensions 200x200. The
  // child frame after scrolling is positioned at 100,100 within the parent
  // frame and is clipped to 100x100. The grand child's main frame document
  // position is then 200,200 after the child frame is scrolled.
  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(200, 200, 100, 100));

  EXPECT_TRUE(ExecJs(child_frame, "window.scroll(4900, 4900); "));

  waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PageLCPStopsUponInput) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Waiter to ensure main content is loaded.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);

  //  Waiter to ensure that iframe content is loaded.
  auto waiter2 = CreatePageLoadMetricsTestWaiter();
  waiter2->AddPageExpectation(TimingField::kLoadEvent);
  waiter2->AddSubFrameExpectation(TimingField::kLoadEvent);
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kLargestContentfulPaint);
  waiter2->AddSubFrameExpectation(TimingField::kLargestContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kFirstInputOrScroll);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/click_to_create_iframe.html"));
  waiter->Wait();

  // Tap in the middle of the button.
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGesture(
      RenderFrameHost(),
      "var submitRect = document.getElementById('button')"
      ".getBoundingClientRect();"));
  double y;
  ASSERT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractDouble(
      RenderFrameHost(),
      "window.domAutomationController.send((submitRect.top +"
      "submitRect.bottom) / 2);",
      &y));
  double x;
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractDouble(
      RenderFrameHost(),
      "window.domAutomationController.send((submitRect.left + submitRect.right)"
      "/ 2);",
      &x));
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft,
      gfx::Point(static_cast<int>(x), static_cast<int>(y)));
  waiter2->Wait();

  // LCP is collected only at the end of the page lifecycle. Navigate to flush.
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 1);
  auto all_frames_value =
      histogram_tester_
          ->GetAllSamples(internal::kHistogramLargestContentfulPaint)[0]
          .min;

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramLargestContentfulPaintMainFrame, 1);
  auto main_frame_value =
      histogram_tester_
          ->GetAllSamples(
              internal::kHistogramLargestContentfulPaintMainFrame)[0]
          .min;
  // Even though the content on the iframe is larger, the all_frames LCP value
  // should match the main frame value because the iframe content was created
  // after input in the main frame.
  ASSERT_EQ(all_frames_value, main_frame_value);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, FirstInputDelayFromClick) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  auto waiter2 = CreatePageLoadMetricsTestWaiter();
  waiter2->AddPageExpectation(TimingField::kLoadEvent);
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kFirstInputDelay);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/click.html"));
  waiter->Wait();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter2->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputTimestamp,
                                      1);
}

// Tests that a portal activation records metrics.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PortalActivation) {
  // We only record metrics for portals when the time is consistent across
  // processes.
  if (!base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("portal.test", "/title1.html"));
  content::WebContents* outer_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a portal to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::string script = R"(
    var portal = document.createElement('portal');
    portal.src = '%s';
    document.body.appendChild(portal);
  )";
  content::WebContentsAddedObserver contents_observer;
  content::TestNavigationObserver portal_nav_observer(a_url);
  portal_nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(outer_contents,
                     base::StringPrintf(script.c_str(), a_url.spec().c_str())));
  portal_nav_observer.WaitForNavigationFinished();
  content::WebContents* portal_contents = contents_observer.GetWebContents();

  {
    // The portal is not activated, so nothing should be recorded.
    auto entries = test_ukm_recorder_->GetMergedEntriesByName(
        ukm::builders::PageLoad::kEntryName);
    EXPECT_EQ(0u, entries.size());
  }

  // Activate the portal.
  std::string activated_listener = R"(
    activatePromise = new Promise(resolve => {
      window.addEventListener('portalactivate', resolve(true));
    });
  )";
  EXPECT_TRUE(ExecJs(portal_contents, activated_listener));
  EXPECT_TRUE(
      ExecJs(outer_contents, "document.querySelector('portal').activate()"));

  EXPECT_EQ(true,
            EvalJsWithManualReply(portal_contents,
                                  "activatePromise.then(r => { "
                                  "  window.domAutomationController.send(r);"
                                  "});"));

  // The activated portal contents should be the currently active contents.
  EXPECT_EQ(portal_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(portal_contents, outer_contents);

  {
    // The portal is activated, so there should be a PageLoad entry.
    auto entries = test_ukm_recorder_->GetMergedEntriesByName(
        ukm::builders::PageLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
  }
  {
    // The portal is activated, also check the portal entry.
    auto entries = test_ukm_recorder_->GetMergedEntriesByName(
        ukm::builders::Portal_Activate::kEntryName);
    EXPECT_EQ(1u, entries.size());
  }
}

class PageLoadMetricsBrowserTestWithBackForwardCache
    : public PageLoadMetricsBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          // Set a very long TTL before expiration (longer than the test
          // timeout) so tests that are expecting deletion don't pass when they
          // shouldn't.
          //
          // TODO(hajimehoshi): This value is used in various places. Define a
          // constant and use it.
          //
          // Some features like the outstanding network requests are expected to
          // appear in almost any output. Filter them out to make the tests
          // simpler.
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"},
           {"ignore_outstanding_network_request_for_testing", "true"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithBackForwardCache,
                       BackForwardCacheEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  auto url2 = embedded_test_server()->GetURL("b.com", "/title1.html");

  // Go to URL1.
  ui_test_utils::NavigateToURL(browser(), url1);

  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache, 0);
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache, 0);

  // Go to URL2. The previous page (URL1) is put into the back-forward cache.
  ui_test_utils::NavigateToURL(browser(), url2);

  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache, 1);
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache, 0);

  // Go back to URL1. The previous page (URL2) is put into the back-forward
  // cache.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache, 2);

  // For now UmaPageLoadMetricsObserver::OnEnterBackForwardCache returns
  // STOP_OBSERVING, OnRestoreFromBackForward is never reached.
  //
  // TODO(hajimehoshi): Update this when the UmaPageLoadMetricsObserver
  // continues to observe after entering to back-forward cache.
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, EarlyHints) {
  content::NavigationHandleTiming timing =
      NavigateWithEarlyHints(EarlyHintsPolicy::OneHint);

  // There were no redirections, so the first request should be the same as the
  // final request.
  EXPECT_FALSE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_FALSE(timing.early_hints_for_final_request_time.is_null());
  EXPECT_EQ(timing.early_hints_for_first_request_time,
            timing.early_hints_for_final_request_time);

  // The Early Hints response (informational response) start time should be
  // equal to the response start time.
  EXPECT_FALSE(timing.first_response_start_time.is_null());
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_EQ(timing.first_response_start_time,
            timing.early_hints_for_first_request_time);
  EXPECT_EQ(timing.final_response_start_time,
            timing.early_hints_for_first_request_time);
  // The non-informational response start time should be recorded separately.
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_LT(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints response should be recorded.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 1);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page.html"));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, EarlyHints_NoHints) {
  content::NavigationHandleTiming timing =
      NavigateWithEarlyHints(EarlyHintsPolicy::NoHints);

  // No Early Hints responses were received.
  EXPECT_TRUE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_TRUE(timing.early_hints_for_final_request_time.is_null());
  // There were no informational responses, so the final response start time
  // should be equal to the final non-informational response start time.
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_EQ(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints response should not be recorded.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 0);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page.html"));
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, EarlyHints_MultipleHints) {
  content::NavigationHandleTiming timing =
      NavigateWithEarlyHints(EarlyHintsPolicy::MultipleHints);

  // There were no redirections, so the first request should be the same as the
  // final request.
  EXPECT_FALSE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_FALSE(timing.early_hints_for_final_request_time.is_null());
  EXPECT_EQ(timing.early_hints_for_first_request_time,
            timing.early_hints_for_final_request_time);

  // The Early Hints response (informational response) start time should be
  // equal to the response start time.
  EXPECT_FALSE(timing.first_response_start_time.is_null());
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_EQ(timing.first_response_start_time,
            timing.early_hints_for_first_request_time);
  EXPECT_EQ(timing.final_response_start_time,
            timing.early_hints_for_first_request_time);
  // The non-informational response start time should be recorded separately.
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_LT(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints responses should be recorded only one time.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 1);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page.html"));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       EarlyHints_HintsBeforeRedirection) {
  content::NavigationHandleTiming timing = RedirectWithEarlyHints(
      EarlyHintsPolicy::MultipleHints, EarlyHintsPolicy::NoHints);

  // The early hints were served for the first request, but not for the
  // redirected request.
  EXPECT_FALSE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_TRUE(timing.early_hints_for_final_request_time.is_null());

  // There were no informational responses for the redirected request, so the
  // final response start time should be equal to the final non-informational
  // response start time.
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_EQ(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints response should be recorded only for the
  // first request.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 0);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page2.html"));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       EarlyHints_HintsAfterRedirection) {
  content::NavigationHandleTiming timing = RedirectWithEarlyHints(
      EarlyHintsPolicy::NoHints, EarlyHintsPolicy::MultipleHints);

  // The early hints were served for the redirected request, but not for the
  // first request.
  EXPECT_TRUE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_FALSE(timing.early_hints_for_final_request_time.is_null());

  // The Early Hints response (informational response) start time should be
  // equal to the final response start time.
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_EQ(timing.final_response_start_time,
            timing.early_hints_for_final_request_time);
  // The non-informational response start time should be recorded separately.
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_LT(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints response should be recorded only for the
  // redirected request.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 1);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page2.html"));
    EXPECT_FALSE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       EarlyHints_HintsBeforeAndAfterRedirection) {
  content::NavigationHandleTiming timing = RedirectWithEarlyHints(
      EarlyHintsPolicy::MultipleHints, EarlyHintsPolicy::MultipleHints);

  // The early hints were served for both the first request and the redirected
  // request.
  EXPECT_FALSE(timing.early_hints_for_first_request_time.is_null());
  EXPECT_FALSE(timing.early_hints_for_final_request_time.is_null());
  EXPECT_LT(timing.early_hints_for_first_request_time,
            timing.early_hints_for_final_request_time);

  // The Early Hints response (informational response) start time should be
  // equal to the final response start time.
  EXPECT_FALSE(timing.final_response_start_time.is_null());
  EXPECT_EQ(timing.final_response_start_time,
            timing.early_hints_for_final_request_time);
  // The non-informational response start time should be recorded separately.
  EXPECT_FALSE(timing.final_non_informational_response_start_time.is_null());
  EXPECT_LT(timing.final_response_start_time,
            timing.final_non_informational_response_start_time);

  // The timings of the Early Hints response should be recorded.
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFirstRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsFinalRequestStartToEarlyHints, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramEarlyHintsEarlyHintsToFinalResponseStart, 1);

  using NavigationTiming = ukm::builders::NavigationTiming;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(NavigationTiming::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(
        kv.second.get(), embedded_test_server()->GetURL("/mock_page2.html"));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFirstRequestName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), NavigationTiming::kEarlyHintsForFinalRequestName));
  }
}

class NavigationPageLoadMetricsBrowserTest
    : public PageLoadMetricsBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  NavigationPageLoadMetricsBrowserTest() = default;
  ~NavigationPageLoadMetricsBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        page_load_metrics::kPageLoadMetricsTimerDelayFeature,
        {
            // Set a very long TTL before expiration (longer than the test
            // timeout) so tests that are expecting deletion don't pass when
            // they shouldn't.
            {"BufferTimerDelayMillis", "100000"},
        });
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(NavigationPageLoadMetricsBrowserTest, FirstInputDelay) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL(
      (GetParam() == "SameSite") ? "a.com" : "b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  internal::kHistogramFirstContentfulPaint),
              testing::IsEmpty());

  // 1) Navigate to url1.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url1));
  content::RenderFrameHost* rfh_a = RenderFrameHost();
  content::RenderProcessHost* rfh_a_process = rfh_a->GetProcess();

  // Simulate mouse click. FirstInputDelay won't get updated immediately.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));
  // Run arbitrary script and run tasks in the brwoser to ensure the input is
  // processed in the renderer.
  EXPECT_TRUE(content::ExecJs(rfh_a, "var foo = 42;"));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);

  // 2) Immediately navigate to url2.
  if (GetParam() == "CrossSiteRendererInitiated") {
    EXPECT_TRUE(content::NavigateToURLFromRenderer(web_contents(), url2));
  } else {
    EXPECT_TRUE(content::NavigateToURL(web_contents(), url2));
  }

  content::FetchHistogramsFromChildProcesses();
  if (GetParam() != "CrossSiteBrowserInitiated" ||
      rfh_a_process == RenderFrameHost()->GetProcess()) {
    // - For "SameSite" case, since the old and new RenderFrame either share a
    // process (with RenderDocument/back-forward cache) or the RenderFrame is
    // reused the metrics update will be sent to the browser during commit and
    // won't get ignored, successfully updating the FirstInputDelay histogram.
    // - For "CrossSiteRendererInitiated" case, FirstInputDelay was sent when
    // the renderer-initiated navigation started on the old frame.
    // - For "CrossSiteBrowserInitiated" case, if the old and new RenderFrame
    // share a process, the metrics update will be sent to the browser during
    // commit and won't get ignored, successfully updating the histogram.
    histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 1);
  } else {
    // Note that in some cases the metrics might flakily get updated in time,
    // before the browser changed the current RFH. So, we can neither expect it
    // to be 0 all the time or 1 all the time.
    // TODO(crbug.com/1150242): Support updating metrics consistently on
    // cross-RFH cross-process navigations.
  }
}

std::vector<std::string> NavigationPageLoadMetricsBrowserTestTestValues() {
  return {"SameSite", "CrossSiteRendererInitiated",
          "CrossSiteBrowserInitiated"};
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationPageLoadMetricsBrowserTest,
    testing::ValuesIn(NavigationPageLoadMetricsBrowserTestTestValues()));

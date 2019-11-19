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
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/aborts_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/no_state_prefetch_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/session_restore_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/page_load_metrics/browser/observers/core_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
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
        {ukm::kUkmFeature, blink::features::kPortals}, {});
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
    auto entries = histogram_tester_.GetTotalCountsForPrefix("PageLoad.");
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
        histogram_tester_.GetTotalCountsForPrefix("PageLoad.").size();
    size_t total_internal_histograms =
        histogram_tester_.GetTotalCountsForPrefix("PageLoad.Internal.").size();
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
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(prerender_manager);

    prerender::test_utils::TestPrerenderContentsFactory*
        prerender_contents_factory =
            new prerender::test_utils::TestPrerenderContentsFactory();
    prerender_manager->SetPrerenderContentsFactoryForTest(
        prerender_contents_factory);

    content::SessionStorageNamespace* storage_namespace =
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .GetDefaultSessionStorageNamespace();
    ASSERT_TRUE(storage_namespace);

    std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
        prerender_contents_factory->ExpectPrerenderContents(
            prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

    std::unique_ptr<prerender::PrerenderHandle> prerender_handle =
        prerender_manager->AddPrerenderFromOmnibox(url, storage_namespace,
                                                   gfx::Size(640, 480));
    ASSERT_EQ(prerender_handle->contents(), test_prerender->contents());

    // The final status may be either  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED or
    // FINAL_STATUS_RECENTLY_VISITED.
    test_prerender->contents()->set_skip_final_checks(true);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramParseDuration, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_.ExpectTotalCount(internal::kHistogramPageLoadTotalBytes, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  using PageLoad = ukm::builders::PageLoad;
  const auto& entries =
      test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(), url);
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(),
        PageLoad::kDocumentTiming_NavigationToDOMContentLoadedEventFiredName));
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
        kv.second.get(), PageLoad::kMainFrameResource_HttpProtocolSchemeName));
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        kv.second.get(), PageLoad::kSiteEngagementScoreName));
  }

  const auto& nostate_prefetch_entries =
      test_ukm_recorder_->GetMergedEntriesByName(NoStatePrefetch::kEntryName);
  EXPECT_EQ(0u, nostate_prefetch_entries.size());

  // Verify that NoPageLoadMetricsRecorded returns false when PageLoad metrics
  // have been recorded.
  EXPECT_FALSE(NoPageLoadMetricsRecorded());
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
  histogram_tester_.ExpectTotalCount(internal::kHistogramPageLoadTotalBytes, 1);
  histogram_tester_.ExpectTotalCount(
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

  // Verify that NoPageLoadMetricsRecorded returns false when PageLoad metrics
  // have been recorded.
  EXPECT_FALSE(NoPageLoadMetricsRecorded());
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

  // Reset the UKM recorder so it would only contain the cached pageload.
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
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPaintForEmptyDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     0);
}

// TODO(crbug.com/986642): Flaky on Win and Linux.
#if defined(OS_WIN) || defined(OS_LINUX)
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
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddSubFrameExpectation(TimingField::kFirstLayout);
  waiter->AddSubFrameExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframe.html"));
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInDynamicChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/dynamic_iframe.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMultipleChildFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframes.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);

  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMainAndChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL(
      "/page_load_metrics/main_frame_with_iframe.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(), a_url);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);

  // Perform a same-document navigation. No additional metrics should be logged.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html#hash"));
  NavigateToUntrackedUrl();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameUrlNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 1);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstLayout);
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  waiter->Wait();

  // We expect one histogram sample for each navigation to title1.html.
  histogram_tester_.ExpectTotalCount(internal::kHistogramDomContentLoaded, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramLoad, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 2);
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteBlock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);

  // Reload should not log the histogram as the script is not blocked.
  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kDocumentWriteBlockReload);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 1);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kDocumentWriteBlockReload);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteAsync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_async_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_external_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWriteScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html"));
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramDocWriteBlockCount, 0);
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 0);

  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kErrorEvents,
      page_load_metrics::ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_ORDER_FIRST_LAYOUT_FIRST_PAINT, 1);
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

  histogram_tester_.ExpectTotalCount(
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

  histogram_tester_.ExpectTotalCount(
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramAbortCloseBeforeCommit,
                                     1);
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

  histogram_tester_.ExpectTotalCount(
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

  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix("PageLoad.Experimental.AbortTiming.")
                  .empty());
}

// TODO(crbug.com/1009885): Flaky on Linux MSan builds.
#if defined(MEMORY_SANITIZER) && defined(OS_LINUX)
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

  histogram_tester_.ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstMeaningfulPaint,
                                     1);
  histogram_tester_.ExpectTotalCount(
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                     1);
  histogram_tester_.ExpectUniqueSample(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstMeaningfulPaint,
                                     0);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoStatePrefetchObserverCacheable) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 0);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NoStatePrefetchObserverNoStore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/nostore.html"));

  waiter->Wait();

  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.NoStore.Visible", 1);
  histogram_tester_.ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 0);
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramPageLoadTotalBytes, 1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the main HTML response).
  histogram_tester_.ExpectBucketCount(internal::kHistogramPageLoadTotalBytes,
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramPageLoadTotalBytes, 1);

  // Verify that there is a single sample recorded in the 10kB bucket (the size
  // of the iframe response).
  histogram_tester_.ExpectBucketCount(internal::kHistogramPageLoadTotalBytes,
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

  histogram_tester_.ExpectUniqueSample(internal::kHistogramPageLoadTotalBytes,
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

  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(
          WebFeature::kApplicationCacheManifestSelectSecureOrigin),
      1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(
          WebFeature::kApplicationCacheManifestSelectSecureOrigin),
      1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                      1);
  // CSSPropertyID::kFontSize
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_.ExpectBucketCount(
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

  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentAudio), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentImage), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kMixedContentVideo), 1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                      1);
  // CSSPropertyID::kFontSize
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_.ExpectBucketCount(
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

  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kFullscreenInsecureOrigin), 1);
  histogram_tester_.ExpectBucketCount(
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
  EXPECT_EQ(4u, entries.size());
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
  EXPECT_EQ(7u, entries.size());
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

  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_.ExpectBucketCount(
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

  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramName,
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_.ExpectBucketCount(
      internal::kFeaturesHistogramMainFrameName,
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                      1);
  // CSSPropertyID::kFontSize
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 6,
                                      1);
  // CSSPropertyID::kFontSize
  histogram_tester_.ExpectBucketCount(internal::kCssPropertiesHistogramName, 7,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_.ExpectBucketCount(
      internal::kAnimatedCssPropertiesHistogramName, 91, 1);
  histogram_tester_.ExpectBucketCount(
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

  histogram_tester_.ExpectBucketCount(
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
#if defined(OS_CHROMEOS)
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(profile));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
#endif

    std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));
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
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
        internal::kHistogramSessionRestoreForegroundTabFirstContentfulPaint,
        expected_total_count);
    histogram_tester_.ExpectTotalCount(
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
  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfMultipleRestoredTabs) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 2);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, false, 1);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, 4);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadStartedInForeground, true, 2);
  histogram_tester_.ExpectBucketCount(
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
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

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

#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_InitialForegroundTabChanged DISABLED_InitialForegroundTabChanged
#else
#define MAYBE_InitialForegroundTabChanged InitialForegroundTabChanged
#endif
IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       MAYBE_InitialForegroundTabChanged) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

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
  histogram_tester_.ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 0, 1);
  histogram_tester_.ExpectBucketCount(internal::kHistogramPageLoadCacheBytes, 0,
                                      1);

  // Force histograms to record.
  NavigateToUntrackedUrl();

  // Verify that the cached resource from the memory cache is recorded
  // correctly.
  histogram_tester_.ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 1, 1);
  histogram_tester_.ExpectBucketCount(internal::kHistogramPageLoadCacheBytes,
                                      10, 1);
}

// Verifies that css image resources shared across document do not cause a
// crash, and are only counted once per context. https://crbug.com/979459.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MemoryCacheResources_RecordedOncePerContext) {
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
  histogram_tester_.ExpectBucketCount(
      internal::kHistogramCacheCompletedResources, 2, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, InputEventsForClick) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/page_load_metrics/link.html"));
  waiter->Wait();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, InputEventsForOmniboxMatch) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), embedded_test_server()->GetURL("/title1.html").spec(),
      base::TimeTicks::Now());
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       InputEventsForJavaScriptHref) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/page_load_metrics/javascript_href.html"));
  waiter->Wait();
  waiter = CreatePageLoadMetricsTestWaiter();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       InputEventsForJavaScriptWindowOpen) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/javascript_window_open.html"));
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

  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
  histogram_tester_.ExpectTotalCount(internal::kHistogramInputToFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);
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
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstInputTimestamp,
                                     0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PortalPageLoad_IsNotLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* outer_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL portal_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  std::string script = R"(
    var portal = document.createElement('portal');
    portal.src = '%s';
    document.body.appendChild(portal);
  )";

  content::TestNavigationObserver portal_nav_observer(portal_url);
  portal_nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecJs(outer_contents,
             base::StringPrintf(script.c_str(), portal_url.spec().c_str())));
  portal_nav_observer.WaitForNavigationFinished();

  // For simplicity, use the redirect chain length histogram as a proxy for
  // whether page_load_metrics tracked the navigation, since it is emitted
  // unconditionally on commit.
  histogram_tester_.ExpectTotalCount("PageLoad.Navigation.RedirectChainLength",
                                     1);
}

// Portals that activate and re-navigate should be treated normally.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       NonEmbeddedPortalPageLoad_IsLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* outer_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL portal_url =
      embedded_test_server()->GetURL("portal.test", "/title2.html");
  std::string script = R"(
    var portal = document.createElement('portal');
    portal.src = '%s';
    document.body.appendChild(portal);
  )";

  content::WebContentsAddedObserver contents_observer;
  content::TestNavigationObserver portal_nav_observer(portal_url);
  portal_nav_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecJs(outer_contents,
             base::StringPrintf(script.c_str(), portal_url.spec().c_str())));
  portal_nav_observer.WaitForNavigationFinished();

  // The portal has navigated but is still embedded. Activate it to make it
  // non-embedded.
  content::WebContents* portal_contents = contents_observer.GetWebContents();
  std::string activated_listener = R"(
    activated = false;
    window.addEventListener('portalactivate', e => {
      activated = true;
    });
  )";
  EXPECT_TRUE(ExecJs(portal_contents, activated_listener));
  EXPECT_TRUE(
      ExecJs(outer_contents, "document.querySelector('portal').activate()"));

  std::string activated_poll = R"(
    setInterval(() => {
      if (activated)
        window.domAutomationController.send(true);
    }, 10);
  )";
  EXPECT_EQ(true, EvalJsWithManualReply(portal_contents, activated_poll));

  // The activated portal contents should be the currently active contents.
  // Navigate it again and ensure that page_load_metrics tracks that page load.
  EXPECT_EQ(portal_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(portal_contents, outer_contents);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title3.html"));

  // For simplicity, use the redirect chain length histogram as a proxy for
  // whether page_load_metrics tracked the navigation, since it is emitted
  // unconditionally on commit.
  histogram_tester_.ExpectTotalCount("PageLoad.Navigation.RedirectChainLength",
                                     2);
}

// Does a navigation to a page controlled by a service worker and verifies
// that service worker page load metrics are logged.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, ServiceWorkerMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a page that registers a service worker.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('fetch_event_pass_through.js');"));
  waiter->Wait();

  // The first load was not controlled, so service worker metrics should not be
  // logged.
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 0);

  waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a controlled page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  waiter->Wait();

  // Service worker metrics should be logged.
  histogram_tester_.ExpectTotalCount(internal::kHistogramFirstPaint, 2);
  histogram_tester_.ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
}

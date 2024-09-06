// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_test_utils.h"
#include "chrome/browser/preloading/preview/preview_test_util.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/prerender_histograms.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using page_load_metrics::PageEndReason;
using page_load_metrics::PageLoadMetricsTestWaiter;
using TimingField = page_load_metrics::PageLoadMetricsTestWaiter::TimingField;
using WebFeature = blink::mojom::WebFeature;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using NoStatePrefetch = ukm::builders::NoStatePrefetch;
using PageLoad = ukm::builders::PageLoad;
using HistoryNavigation = ukm::builders::HistoryNavigation;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;

namespace {

constexpr char kCacheablePathPrefix[] = "/cacheable";

const char kResponseWithNoStore[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "\r\n"
    "The server speaks HTTP!";

constexpr char kCreateFrameAtPositionScript[] = R"(
  var new_iframe = document.createElement('iframe');
  new_iframe.src = $1;
  new_iframe.name = 'frame';
  new_iframe.frameBorder = 0;
  new_iframe.setAttribute('style',
      'position:absolute; top:$2; left:$3; width:$4; height:$4;');
  document.body.appendChild(new_iframe);)";

constexpr char kCreateFrameAtPositionNotifyOnLoadScript[] = R"(
  var new_iframe = document.createElement('iframe');
  new_iframe.src = $1;
  new_iframe.name = 'frame';
  new_iframe.frameBorder = 0;
  new_iframe.setAttribute('style',
      'position:absolute; top:$2; left:$3; width:$4; height:$4;');
  new_iframe.onload = function() {
    window.domAutomationController.send('iframe.onload');
  };
  document.body.appendChild(new_iframe);)";

constexpr char kCreateFrameAtTopRightPositionScript[] = R"(
  var new_iframe = document.createElement('iframe');
  new_iframe.src = $1;
  new_iframe.name = 'frame';
  new_iframe.frameBorder = 0;
  new_iframe.setAttribute('style',
      'position:absolute; top:$2; right:$3; width:$4; height:$4;');
  document.body.appendChild(new_iframe);)";

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
        {ukm::kUkmFeature},
        // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        {features::kHttpsUpgrades});
  }

  PageLoadMetricsBrowserTest(const PageLoadMetricsBrowserTest&) = delete;
  PageLoadMetricsBrowserTest& operator=(const PageLoadMetricsBrowserTest&) =
      delete;

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
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  int64_t GetUKMPageLoadMetric(std::string metric_name) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);

    EXPECT_EQ(1ul, merged_entries.size());
    const auto& kv = merged_entries.begin();
    const int64_t* recorded =
        ukm::TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
    EXPECT_TRUE(recorded != nullptr);
    return (*recorded);
  }

  void MakeComponentFullscreen(const std::string& id) {
    EXPECT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "document.getElementById(\"" + id + "\").webkitRequestFullscreen();"));
  }

  std::string GetRecordedPageLoadMetricNames() {
    auto entries = histogram_tester_->GetTotalCountsForPrefix("PageLoad.");
    std::vector<std::string> names = base::ToVector(
        entries, &base::HistogramTester::CountsMap::value_type::first);
    return base::JoinString(names, ",");
  }

  bool NoPageLoadMetricsRecorded() {
    // Determine whether any 'public' page load metrics are recorded. We exclude
    // 'internal' metrics as these may be recorded for debugging purposes, and
    // abandonment-related metrics, which are expected to be recorded for all
    // kinds of navigations.
    size_t total_pageload_histograms =
        histogram_tester_->GetTotalCountsForPrefix("PageLoad.").size();
    size_t total_internal_histograms =
        histogram_tester_->GetTotalCountsForPrefix("PageLoad.Internal.").size();
    size_t total_abandon_histograms =
        histogram_tester_
            ->GetTotalCountsForPrefix(
                internal::kAbandonedPageLoadMetricsHistogramPrefix)
            .size();
    DCHECK_GE(total_pageload_histograms,
              total_internal_histograms + total_abandon_histograms);
    return total_pageload_histograms - total_internal_histograms -
               total_abandon_histograms ==
           0;
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter(
      const char* observer_name,
      content::WebContents* web_contents = nullptr) {
    if (!web_contents)
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents,
                                                       observer_name);
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
        no_state_prefetch_manager->AddSameOriginSpeculation(
            url, storage_namespace, gfx::Size(640, 480),
            url::Origin::Create(url));
    ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());

    // The final status may be either  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED or
    // FINAL_STATUS_RECENTLY_VISITED.
    test_prerender->contents()->set_skip_final_checks(true);
  }

  void VerifyBasicPageLoadUkms(const GURL& expected_source_url) {
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

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetPrimaryMainFrame();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PageLCPImagePriority) {
  // Waiter to ensure main content is loaded.
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);

  const char kHtmlHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";
  const char kImgHttpResponseHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  auto main_html_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/mock_page.html",
          false /*relative_url_is_prefix*/);
  auto img_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/images/lcp.jpg",
          false /*relative_url_is_prefix*/);

  ASSERT_TRUE(embedded_test_server()->Start());

  // File is under content/test/data/
  std::string file_contents;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_dir));
    base::FilePath file_name = test_dir.AppendASCII("single_face.jpg");
    ASSERT_TRUE(base::ReadFileToString(file_name, &file_contents));
  }

  browser()->OpenURL(
      content::OpenURLParams(embedded_test_server()->GetURL("/mock_page.html"),
                             content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  main_html_response->WaitForRequest();
  main_html_response->Send(kHtmlHttpResponseHeader);
  main_html_response->Send(
      "<html><body><img src=\"/images/lcp.jpg\"></body></html>");
  main_html_response->Done();

  img_response->WaitForRequest();

  // Force layout and thus the visibility-based priority to be set, before the
  // loading is finished.
  content::EvalJsResult result =
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
      new Promise(resolve => {
        const forceLayout = () => {
          document.querySelector('img').offsetTop;
          resolve();
        };
        if (document.querySelector('img')) {
          forceLayout();
        } else {
          // Wait for DOMContentLoaded to ensure <img> is inserted.
          document.addEventListener('DOMContentLoaded', forceLayout);
        }
      })
  )");
  EXPECT_EQ("", result.error);

  img_response->Send(kImgHttpResponseHeader);
  img_response->Send(file_contents);
  img_response->Done();

  // Wait on an LCP entry to make sure we have one to report when navigating
  // away.
  content::EvalJsResult result2 =
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
 (async () => {
   await new Promise(resolve => {
     (new PerformanceObserver(list => {
       const entries = list.getEntries();
       for (let entry of entries) {
         if (entry.url.includes('images')) {resolve()}
       }
     }))
     .observe({type: 'largest-contentful-paint', buffered: true});
 })})())");
  EXPECT_EQ("", result2.error);
  waiter->Wait();

  // LCP is collected only at the end of the page lifecycle. Navigate to
  // flush.
  NavigateToUntrackedUrl();

  // Image should be loaded with `net::MEDIUM` priority because the image is
  // visible.
  int64_t value = GetUKMPageLoadMetric(
      PageLoad::PageLoad::
          kPaintTiming_LargestContentfulPaintRequestPriorityName);
  ASSERT_EQ(value, static_cast<int>(net::MEDIUM));
}

class PageLoadMetricsBrowserTestAnimatedLCP
    : public PageLoadMetricsBrowserTest {
 protected:
  void test_animated_image_lcp(bool smaller, bool animated) {
    // Waiter to ensure main content is loaded.
    auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
    waiter->AddPageExpectation(TimingField::kLoadEvent);
    waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
    waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);

    const char kHtmlHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n";
    const char kImgHttpResponseHeader[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "\r\n";
    auto main_html_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_page.html",
            false /*relative_url_is_prefix*/);
    auto img_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(),
            animated ? "/images/animated-delayed.png" : "/images/delayed.jpg",
            false /*relative_url_is_prefix*/);

    ASSERT_TRUE(embedded_test_server()->Start());

    // File is under content/test/data/
    const std::string file_name_string =
        animated ? "animated.png" : "single_face.jpg";
    std::string file_contents;
    // The first_frame_size number for the animated case (262), represents the
    // first frame of the animated PNG + an extra chunk enabling the decoder to
    // understand the first frame is done and decode it.
    // For the non-animated case (5000), it's an arbitrary number that
    // represents a part of the JPEG's frame.
    const unsigned first_frame_size = animated ? 262 : 5000;

    // Read the animated image into two frames.
    {
      base::ScopedAllowBlockingForTesting allow_io;
      base::FilePath test_dir;
      ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_dir));
      base::FilePath file_name = test_dir.AppendASCII(file_name_string);
      ASSERT_TRUE(base::ReadFileToString(file_name, &file_contents));
    }
    // Split the contents into 2 frames
    std::string first_frame = file_contents.substr(0, first_frame_size);
    std::string second_frame = file_contents.substr(first_frame_size);

    browser()->OpenURL(
        content::OpenURLParams(
            embedded_test_server()->GetURL("/mock_page.html"),
            content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});

    main_html_response->WaitForRequest();
    main_html_response->Send(kHtmlHttpResponseHeader);
    main_html_response->Send(
        animated ? "<html><body></body><img "
                   "src=\"/images/animated-delayed.png\"></script></html>"
                 : "<html><body></body><img "
                   "src=\"/images/delayed.jpg\"></script></html>");
    main_html_response->Done();

    img_response->WaitForRequest();
    img_response->Send(kImgHttpResponseHeader);
    img_response->Send(first_frame);

    // Trigger a double rAF and wait a bit, then take a timestamp that's after
    // the presentation time of the first frame.
    // Then wait some more to ensure the timestamp is not too close to the point
    // where the second frame is sent.
    content::EvalJsResult result =
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
(async () => {
  const double_raf = () => {
    return new Promise(r => {
      requestAnimationFrame(()=>requestAnimationFrame(r));
    })
  };
  await double_raf();
  await new Promise(r => setTimeout(r, 500));
  const timestamp = performance.now();
  await new Promise(r => setTimeout(r, 50));
  return timestamp;
})();)");
    EXPECT_EQ("", result.error);
    double timestamp = result.ExtractDouble();

    img_response->Send(second_frame);
    img_response->Done();

    // Wait on an LCP entry to make sure we have one to report when navigating
    // away.
    content::EvalJsResult result2 =
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
 (async () => {
   await new Promise(resolve => {
     (new PerformanceObserver(list => {
       const entries = list.getEntries();
       for (let entry of entries) {
         if (entry.url.includes('images')) {resolve()}
       }
     }))
     .observe({type: 'largest-contentful-paint', buffered: true});
 })})())");
    EXPECT_EQ("", result2.error);
    waiter->Wait();

    // LCP is collected only at the end of the page lifecycle. Navigate to
    // flush.
    NavigateToUntrackedUrl();

    int64_t value = GetUKMPageLoadMetric(
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name);

    if (smaller) {
      ASSERT_LT(value, timestamp);
    } else {
      ASSERT_GT(value, timestamp);
    }
  }
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

// TODO(crbug.com/40839280): Re-enable this test
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       DISABLED_MainFrameViewportRect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content.html");

  auto main_frame_viewport_rect_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int side_scrollbar_width =
      EvalJs(web_contents,
             "window.innerWidth - document.documentElement.clientWidth")
          .ExtractInt();
  int bottom_scrollbar_height =
      EvalJs(web_contents,
             "window.innerHeight - document.documentElement.clientHeight")
          .ExtractInt();

  content::RenderWidgetHostView* guest_host_view =
      web_contents->GetRenderWidgetHostView();
  gfx::Size viewport_size = guest_host_view->GetVisibleViewportSize();
  viewport_size -= gfx::Size(side_scrollbar_width, bottom_scrollbar_height);

  main_frame_viewport_rect_expectation_waiter
      ->AddMainFrameViewportRectExpectation(
          gfx::Rect(5000, 5000, viewport_size.width(), viewport_size.height()));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(5000, 5000)"));

  main_frame_viewport_rect_expectation_waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersection_RTLPage \
  DISABLED_MainFrameIntersection_RTLPage
#else
#define MAYBE_MainFrameIntersection_RTLPage MainFrameIntersection_RTLPage
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersection_RTLPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content_rtl.html");

  auto main_frame_intersection_rect_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int document_width =
      EvalJs(web_contents, "document.body.scrollWidth").ExtractInt();

  int side_scrollbar_width =
      EvalJs(web_contents,
             "window.innerWidth - document.documentElement.clientWidth")
          .ExtractInt();
  int bottom_scrollbar_height =
      EvalJs(web_contents,
             "window.innerHeight - document.documentElement.clientHeight")
          .ExtractInt();

  content::RenderWidgetHostView* guest_host_view =
      web_contents->GetRenderWidgetHostView();
  gfx::Size viewport_size = guest_host_view->GetVisibleViewportSize();
  viewport_size -= gfx::Size(side_scrollbar_width, bottom_scrollbar_height);

  main_frame_intersection_rect_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(document_width - 100 - 50, 5050, 100, 100));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace(kCreateFrameAtTopRightPositionScript,
                                subframe_url.spec().c_str(), 5050, 50, 100)));

  main_frame_intersection_rect_expectation_waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(
    PageLoadMetricsBrowserTest,
    // TODO(crbug.com/40839452): Re-enable this test
    DISABLED_NonZeroMainFrameScrollOffset_NestedSameOriginFrame_MainFrameIntersection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content.html");

  auto main_frame_intersection_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Subframe
  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/50, /*y=*/5050, /*width=*/100, /*height=*/100));

  // Nested frame
  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/55, /*y=*/5055, /*width=*/1, /*height=*/1));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  content::DOMMessageQueue dom_message_queue(web_contents);
  std::string message;
  content::ExecuteScriptAsync(
      web_contents,
      content::JsReplace(kCreateFrameAtPositionNotifyOnLoadScript,
                         subframe_url.spec().c_str(), 5050, 50, 100));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe.onload\"", message);

  content::RenderFrameHost* subframe = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "frame"));

  GURL nested_frame_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(ExecJs(
      subframe, content::JsReplace(kCreateFrameAtPositionScript,
                                   nested_frame_url.spec().c_str(), 5, 5, 1)));

  main_frame_intersection_expectation_waiter->Wait();
}

// TODO(crbug.com/40234728): Fix flakiness.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_NonZeroMainFrameScrollOffset_NestedCrossOriginFrame_MainFrameIntersection \
  DISABLED_NonZeroMainFrameScrollOffset_NestedCrossOriginFrame_MainFrameIntersection
#else
#define MAYBE_NonZeroMainFrameScrollOffset_NestedCrossOriginFrame_MainFrameIntersection \
  NonZeroMainFrameScrollOffset_NestedCrossOriginFrame_MainFrameIntersection
#endif
IN_PROC_BROWSER_TEST_F(
    PageLoadMetricsBrowserTest,
    MAYBE_NonZeroMainFrameScrollOffset_NestedCrossOriginFrame_MainFrameIntersection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content.html");

  auto main_frame_intersection_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Subframe
  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/50, /*y=*/5050, /*width=*/100, /*height=*/100));

  // Nested frame
  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/55, /*y=*/5055, /*width=*/1, /*height=*/1));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  content::DOMMessageQueue dom_message_queue(web_contents);
  std::string message;
  content::ExecuteScriptAsync(
      web_contents,
      content::JsReplace(kCreateFrameAtPositionNotifyOnLoadScript,
                         subframe_url.spec().c_str(), 5050, 50, 100));
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe.onload\"", message);

  content::RenderFrameHost* subframe = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "frame"));

  GURL nested_frame_url =
      embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(ExecJs(
      subframe, content::JsReplace(kCreateFrameAtPositionScript,
                                   nested_frame_url.spec().c_str(), 5, 5, 1)));

  main_frame_intersection_expectation_waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NonZeroMainFrameScrollOffset_SameOriginFrameAppended_MainFrameIntersection \
  DISABLED_NonZeroMainFrameScrollOffset_SameOriginFrameAppended_MainFrameIntersection
#else
#define MAYBE_NonZeroMainFrameScrollOffset_SameOriginFrameAppended_MainFrameIntersection \
  NonZeroMainFrameScrollOffset_SameOriginFrameAppended_MainFrameIntersection
#endif
IN_PROC_BROWSER_TEST_F(
    PageLoadMetricsBrowserTest,
    MAYBE_NonZeroMainFrameScrollOffset_SameOriginFrameAppended_MainFrameIntersection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content.html");

  auto main_frame_intersection_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/50, /*y=*/5050, /*width=*/1, /*height=*/1));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url = embedded_test_server()->GetURL("a.com", "/title1.html");

  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace(kCreateFrameAtPositionScript,
                                subframe_url.spec().c_str(), 5050, 50, 1)));

  main_frame_intersection_expectation_waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NonZeroMainFrameScrollOffset_CrossOriginFrameAppended_MainFrameIntersection \
  DISABLED_NonZeroMainFrameScrollOffset_CrossOriginFrameAppended_MainFrameIntersection
#else
#define MAYBE_NonZeroMainFrameScrollOffset_CrossOriginFrameAppended_MainFrameIntersection \
  NonZeroMainFrameScrollOffset_CrossOriginFrameAppended_MainFrameIntersection
#endif
IN_PROC_BROWSER_TEST_F(
    PageLoadMetricsBrowserTest,
    MAYBE_NonZeroMainFrameScrollOffset_CrossOriginFrameAppended_MainFrameIntersection) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/scroll/scrollable_page_with_content.html");

  auto main_frame_intersection_expectation_waiter =
      CreatePageLoadMetricsTestWaiter("waiter");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  main_frame_intersection_expectation_waiter
      ->AddMainFrameIntersectionExpectation(
          gfx::Rect(/*x=*/50, /*y=*/5050, /*width=*/1, /*height=*/1));

  ASSERT_TRUE(ExecJs(web_contents, "window.scrollTo(0, 5000)"));

  GURL subframe_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  EXPECT_TRUE(
      ExecJs(web_contents,
             content::JsReplace(kCreateFrameAtPositionScript,
                                subframe_url.spec().c_str(), 5050, 50, 1)));

  main_frame_intersection_expectation_waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NewPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptLoad, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramParseBlockedOnScriptExecution, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);

  VerifyBasicPageLoadUkms(final_url);

  const auto& nostate_prefetch_entries =
      test_ukm_recorder_->GetMergedEntriesByName(NoStatePrefetch::kEntryName);
  EXPECT_EQ(0u, nostate_prefetch_entries.size());

  VerifyNavigationMetrics({final_url});
}

class PageLoadMetricsPre3pcdBrowserTest : public PageLoadMetricsBrowserTest {
 public:
  PageLoadMetricsPre3pcdBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        content_settings::features::kTrackingProtection3pcd);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40931292): Re-enable this test on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NoStatePrefetchMetrics DISABLED_NoStatePrefetchMetrics
#else
#define MAYBE_NoStatePrefetchMetrics NoStatePrefetchMetrics
#endif
// Triggers nostate prefetch, and verifies that the UKM metrics related to
// nostate prefetch are recorded correctly.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsPre3pcdBrowserTest,
                       MAYBE_NoStatePrefetchMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");

  TriggerNoStatePrefetch(url);

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateToUntrackedUrl();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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

// Test that we log kMainFrameResource_RequestHasNoStore when response has
// cache-control:no-store response header.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MainFrameHasNoStore) {
  // Create a HTTP response to control main-frame navigation to send no-store
  // response.
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kUrl = embedded_test_server()->GetURL("/main_document");

  // Load the document and specify no-store for the main resource.
  content::TestNavigationManager navigation_manager(web_contents(), kUrl);
  browser()->OpenURL(content::OpenURLParams(kUrl, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

  // The navigation starts.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // The response's headers are received.
  response.WaitForRequest();
  response.Send(kResponseWithNoStore);
  response.Done();
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  NavigateToUntrackedUrl();

  auto entries =
      test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    auto* const no_store_entry = kv.second.get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(no_store_entry, kUrl);

    // RequestHasNoStore event should be recorded with value 1 as the response
    // as no-store in it.
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        no_store_entry, PageLoad::kMainFrameResource_RequestHasNoStoreName));
    test_ukm_recorder_->ExpectEntryMetric(
        no_store_entry, PageLoad::kMainFrameResource_RequestHasNoStoreName, 1);
  }
}

// Test that we set kMainFrameResource_RequestHasNoStore to false when response
// has no cache-control:no-store header.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MainFrameDoesnotHaveNoStore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an URL to see if metrics are recorded.
  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");

  // Navigate to the |kUrl| with no cache-control: no store header.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  NavigateToUntrackedUrl();

  auto entries =
      test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto& kv : entries) {
    auto* const no_store_entry = kv.second.get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(no_store_entry, kUrl);

    // RequestHasNoStore event should be recorded with value false.
    EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(
        no_store_entry, PageLoad::kMainFrameResource_RequestHasNoStoreName));
    test_ukm_recorder_->ExpectEntryMetric(
        no_store_entry, PageLoad::kMainFrameResource_RequestHasNoStoreName, 0);
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
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoPaintForEmptyDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      0);
}

// TODO(crbug.com/41472183): Flaky on Win and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddSubFrameExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
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
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInDynamicChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/dynamic_iframe.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PaintInMultipleChildFrames) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL("/page_load_metrics/iframes.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);

  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

// TODO(crbug.com/334416161): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PaintInMainAndChildFrame DISABLED_PaintInMainAndChildFrame
#else
#define MAYBE_PaintInMainAndChildFrame PaintInMainAndChildFrame
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_PaintInMainAndChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL a_url(embedded_test_server()->GetURL(
      "/page_load_metrics/main_frame_with_iframe.html"));

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstPaint);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstPaint);
  waiter->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  // Perform a same-document navigation. No additional metrics should be logged.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html#hash")));
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  VerifyNavigationMetrics({url});
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameUrlNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramLoad, 1);

  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/doc_write_aborts_subframe.html")));
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  EXPECT_FALSE(waiter->DidObserveInPage(TimingField::kFirstPaint));
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NonHtmlMainResource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/circle.svg")));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

// TODO(crbug.com/40774566): Test flakes on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_NonHttpOrHttpsUrl DISABLED_NonHttpOrHttpsUrl
#else
#define MAYBE_NonHttpOrHttpsUrl NonHttpOrHttpsUrl
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_NonHttpOrHttpsUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, HttpErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/404.html")));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(observer.is_error());
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, Ignore204Pages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/page204.html")));
  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, IgnoreDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DownloadTestObserverTerminal downloads_observer(
      browser()->profile()->GetDownloadManager(),
      1,  // == wait_count (only waiting for "download-test3.gif").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/download-test3.gif")));
  downloads_observer.WaitForFinished();

  NavigateToUntrackedUrl();
  EXPECT_TRUE(NoPageLoadMetricsRecorded())
      << "Recorded metrics: " << GetRecordedPageLoadMetricNames();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWrite) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
}

// Flaky on lacros. See https://crbug.com/1484915
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DocumentWriteBlock DISABLED_DocumentWriteBlock
#else
#define MAYBE_DocumentWriteBlock DocumentWriteBlock
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_DocumentWriteBlock) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
}


// TODO(crbug.com/40931345, crbug.com/334416161): Re-enable this test on Lacros
// and Windows.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
#define MAYBE_DocumentWriteReload DISABLED_DocumentWriteReload
#else
#define MAYBE_DocumentWriteReload DocumentWriteReload
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_DocumentWriteReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);

  // Reload should not log the histogram as the script is not blocked.
  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html")));
  waiter->Wait();

  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_script_block.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteAsync) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_async_script.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DocumentWriteSameDomain) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/document_write_external_script.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, NoDocumentWriteScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/document_write_no_script.html")));
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
}

// TODO(crbug.com/40516222): Flaky on Linux dbg.
// TODO(crbug.com/41328109): Now flaky on Win and Mac.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, DISABLED_BadXhtml) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // When an XHTML page contains invalid XML, it causes a paint of the error
  // message without a layout. Page load metrics currently treats this as an
  // error. Eventually, we'll fix this by special casing the handling of
  // documents with non-well-formed XML on the blink side. See crbug.com/627607
  // for more.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/badxml.xhtml")));
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

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/large.html")));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, PayloadSizeChildFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/large_iframe.html")));
  waiter->Wait();

  // Payload histograms are only logged when a page load terminates, so force
  // navigation to another page.
  NavigateToUntrackedUrl();
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       PayloadSizeIgnoresDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DownloadTestObserverTerminal downloads_observer(
      browser()->profile()->GetDownloadManager(),
      1,  // == wait_count (only waiting for "download-test1.lib").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/download_anchor_click.html")));
  downloads_observer.WaitForFinished();

  NavigateToUntrackedUrl();
}

// Test UseCounter Features observed in the main frame are recorded, exactly
// once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 6, 1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 7, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.CSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       91, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.AnimatedCSSProperties",
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kMixedContentAudio), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kMixedContentImage), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kMixedContentVideo), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 6, 1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 7, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.CSSProperties",
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL("/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       91, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.AnimatedCSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInNonSecureMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "non-secure.test", "/page_load_metrics/use_counter_features.html")));
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kFullscreenInsecureOrigin), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kFullscreenInsecureOrigin), 1);
}

// Test UseCounter UKM features observed.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterUkmFeaturesLogged) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure that the previous page won't be stored in the back/forward cache, so
  // that the histogram will be recorded when the previous page is unloaded.
  // UKM/UMA logging after BFCache eviction is checked by
  // PageLoadMetricsBrowserTestWithBackForwardCache's
  // UseCounterUkmFeaturesLoggedOnBFCacheEviction test.
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url = embedded_test_server()->GetURL(
      "/page_load_metrics/use_counter_features.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_THAT(entries, SizeIs(3));
  std::vector<int64_t> ukm_features;
  for (const ukm::mojom::UkmEntry* entry : entries) {
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
                  static_cast<int64_t>(WebFeature::kNavigatorVibrate)));
}

// Test UseCounter UKM mixed content features observed.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithAutoupgradesDisabled,
                       UseCounterUkmMixedContentFeaturesLogged) {
  // As with UseCounterFeaturesMixedContent, load on a real HTTPS server to
  // trigger mixed content.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  // Ensure that the previous page won't be stored in the back/forward cache, so
  // that the histogram will be recorded when the previous page is unloaded.
  // UKM/UMA logging after BFCache eviction is checked by
  // PageLoadMetricsBrowserTestWithBackForwardCache's
  // UseCounterUkmFeaturesLoggedOnBFCacheEviction test.
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .GetBackForwardCache()
      .DisableForTesting(content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  GURL url =
      https_server.GetURL("/page_load_metrics/use_counter_features.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  MakeComponentFullscreen("testvideo");
  waiter->Wait();
  NavigateToUntrackedUrl();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_THAT(entries, SizeIs(6));
  std::vector<int64_t> ukm_features;
  for (const ukm::mojom::UkmEntry* entry : entries) {
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
                  static_cast<int64_t>(WebFeature::kMixedContentImage),
                  static_cast<int64_t>(WebFeature::kMixedContentAudio),
                  static_cast<int64_t>(WebFeature::kMixedContentVideo)));
}

// Test UseCounter Features observed in a child frame are recorded, exactly
// once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, UseCounterFeaturesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframe.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

// Test UseCounter Features observed in multiple child frames are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterFeaturesInMultipleIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
  // No feature but page visits should get counted.
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kTextWholeText), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kV8Element_Animate_Method), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 0);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.MainFrame.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

// Test UseCounter CSS properties observed in a child frame are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframe.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 6, 1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 7, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.CSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS Properties observed in multiple child frames are
// recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterCSSPropertiesInMultipleIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kFontFamily
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 6, 1);
  // CSSPropertyID::kFontSize
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.CSSProperties", 7, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.CSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS properties observed in a child frame are recorded,
// exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframe.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       91, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.AnimatedCSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter CSS Properties observed in multiple child frames are
// recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterAnimatedCSSPropertiesInMultipleIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  // CSSPropertyID::kWidth
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       161, 1);
  // CSSPropertyID::kMarginLeft
  histogram_tester_->ExpectBucketCount("Blink.UseCounter.AnimatedCSSProperties",
                                       91, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.AnimatedCSSProperties",
      blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
}

// Test UseCounter Features observed for SVG pages.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterObserveSVGImagePage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/circle.svg")));
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 1);
}

// Test UseCounter Permissions Policy Usages in main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterPermissionsPolicyUsageInMainFrame) {
  auto test_feature = static_cast<blink::UseCounterFeature::EnumValue>(
      blink::mojom::PermissionsPolicyFeature::kFullscreen);

  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddUseCounterFeatureExpectation({
      blink::mojom::UseCounterFeatureType::kPermissionsPolicyViolationEnforce,
      test_feature,
  });
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/use_counter_features.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Violation.Enforce", test_feature, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Header2", test_feature, 1);
}

// Test UseCounter Permissions Policy Usages observed in child frame
// are recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterPermissionsPolicyUsageInIframe) {
  auto test_feature = static_cast<blink::UseCounterFeature::EnumValue>(
      blink::mojom::PermissionsPolicyFeature::kFullscreen);

  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddUseCounterFeatureExpectation({
      blink::mojom::UseCounterFeatureType::kPermissionsPolicyViolationEnforce,
      test_feature,
  });
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframe.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Violation.Enforce", test_feature, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Header2", test_feature, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Allow2", test_feature, 1);
}

// Test UseCounter Permissions Policy Usages observed in multiple child frames
// are recorded, exactly once per feature.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       UseCounterPermissionsPolicyUsageInMultipleIframes) {
  auto test_feature = static_cast<blink::UseCounterFeature::EnumValue>(
      blink::mojom::PermissionsPolicyFeature::kFullscreen);

  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddUseCounterFeatureExpectation({
      blink::mojom::UseCounterFeatureType::kPermissionsPolicyViolationEnforce,
      test_feature,
  });
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/use_counter_features_in_iframes.html")));
  waiter->Wait();
  NavigateToUntrackedUrl();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Violation.Enforce", test_feature, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Header2", test_feature, 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.PermissionsPolicy.Allow2", test_feature, 1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, LoadingMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadTimingInfo);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  // Waits until nonzero loading metrics are seen.
  waiter->Wait();
}

class SessionRestorePageLoadMetricsBrowserTest
    : public PageLoadMetricsBrowserTest {
 public:
  SessionRestorePageLoadMetricsBrowserTest() {}

  SessionRestorePageLoadMetricsBrowserTest(
      const SessionRestorePageLoadMetricsBrowserTest&) = delete;
  SessionRestorePageLoadMetricsBrowserTest& operator=(
      const SessionRestorePageLoadMetricsBrowserTest&) = delete;

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
};

class SessionRestorePaintWaiter : public SessionRestoreObserver {
 public:
  SessionRestorePaintWaiter() { SessionRestore::AddObserver(this); }

  SessionRestorePaintWaiter(const SessionRestorePaintWaiter&) = delete;
  SessionRestorePaintWaiter& operator=(const SessionRestorePaintWaiter&) =
      delete;

  ~SessionRestorePaintWaiter() { SessionRestore::RemoveObserver(this); }

  // SessionRestoreObserver implementation:
  void OnWillRestoreTab(content::WebContents* contents) override {
    chrome::InitializePageLoadMetricsForWebContents(contents);
    auto waiter = std::make_unique<PageLoadMetricsTestWaiter>(contents);
    waiter->AddPageExpectation(TimingField::kFirstPaint);
    waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
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
};

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfSingleRestoredTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));
}

IN_PROC_BROWSER_TEST_F(SessionRestorePageLoadMetricsBrowserTest,
                       InitialVisibilityOfMultipleRestoredTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());
}

enum class ReduceTransferSizeUpdatedIPCTestCase {
  kEnabled,
  kDisabled,
};

class PageLoadMetricsResourceLoadBrowserTest
    : public PageLoadMetricsBrowserTest,
      public ::testing::WithParamInterface<
          ReduceTransferSizeUpdatedIPCTestCase> {
 public:
  PageLoadMetricsResourceLoadBrowserTest() {
    if (IsReduceTransferSizeUpdatedIPCEnabled()) {
      feature_list_.InitAndEnableFeature(
          network::features::kReduceTransferSizeUpdatedIPC);
    } else {
      feature_list_.InitAndDisableFeature(
          network::features::kReduceTransferSizeUpdatedIPC);
    }
  }
  ~PageLoadMetricsResourceLoadBrowserTest() override = default;

 protected:
  bool IsReduceTransferSizeUpdatedIPCEnabled() const {
    return GetParam() == ReduceTransferSizeUpdatedIPCTestCase::kEnabled;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PageLoadMetricsResourceLoadBrowserTest,
    testing::ValuesIn({ReduceTransferSizeUpdatedIPCTestCase::kDisabled,
                       ReduceTransferSizeUpdatedIPCTestCase::kEnabled}),
    [](const testing::TestParamInfo<ReduceTransferSizeUpdatedIPCTestCase>&
           info) {
      switch (info.param) {
        case ReduceTransferSizeUpdatedIPCTestCase::kEnabled:
          return "ReduceTransferSizeUpdatedIPCEnabled";
        case ReduceTransferSizeUpdatedIPCTestCase::kDisabled:
          return "ReduceTransferSizeUpdatedIPCDisabled";
      }
    });

// TODO(crbug.com/41412649) Disabled due to flaky timeouts on all platforms.
IN_PROC_BROWSER_TEST_P(PageLoadMetricsResourceLoadBrowserTest,
                       DISABLED_ReceivedAggregateResourceDataLength) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "foo.com", "/cross_site_iframe_factory.html?foo")));
  waiter->Wait();
  int64_t one_frame_page_size = waiter->current_network_bytes();

  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "a.com", "/cross_site_iframe_factory.html?a(b,c,d(e,f,g))")));
  // Verify that 7 iframes are fetched, with some amount of tolerance since
  // favicon is fetched only once.
  waiter->AddMinimumNetworkBytesExpectation(7 * (one_frame_page_size - 100));
  waiter->Wait();
}

IN_PROC_BROWSER_TEST_P(PageLoadMetricsResourceLoadBrowserTest,
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");

  browser()->OpenURL(
      content::OpenURLParams(embedded_test_server()->GetURL("/mock_page.html"),
                             content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  main_response->WaitForRequest();
  main_response->Send(kHttpResponseHeader);
  for (int i = 0; i < kNumChunks; i++) {
    main_response->Send(base::NumberToString(kChunkSize));
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

IN_PROC_BROWSER_TEST_P(PageLoadMetricsResourceLoadBrowserTest,
                       ReceivedCompleteResources) {
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");

  browser()->OpenURL(
      content::OpenURLParams(embedded_test_server()->GetURL("/mock_page.html"),
                             content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

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

  if (!IsReduceTransferSizeUpdatedIPCEnabled()) {
    // When ReduceTransferSizeUpdatedIPC is disabled, network bytes information
    // is sent almost every time when the body data is received. So we can call
    // Wait() before finising `script_response`,
    waiter->Wait();
    script_response->Done();
  } else {
    // But when ReduceTransferSizeUpdatedIPC is enabled, network bytes
    // information is sent only when the resource is complete. So we need to
    // call Wait() after finising `script_response`.
    script_response->Done();
    waiter->Wait();
  }
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

// TODO(crbug.com/334416161): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InputEventsForClick DISABLED_InputEventsForClick
#else
#define MAYBE_InputEventsForClick InputEventsForClick
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, MAYBE_InputEventsForClick) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL("/page_load_metrics/link.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
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

class SoftNavigationBrowserTest : public PageLoadMetricsBrowserTest {
 public:
  void TestSoftNavigation(bool wait_for_second_lcp) {
    StartTracing();
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
    waiter->AddPageExpectation(TimingField::kLoadEvent);
    waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
    waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);
    GURL url = embedded_test_server()->GetURL(
        "/page_load_metrics/soft_navigation.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    waiter->Wait();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());

    waiter->AddPageExpectation(TimingField::kSoftNavigationCountUpdated);
    if (wait_for_second_lcp) {
      waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);
    }

    const std::string wait_for_lcp = R"(
      (async () => {
        await new Promise(
          resolve => {
            (new PerformanceObserver(()=>resolve())).observe(
              {type: 'largest-contentful-paint',
               includeSoftNavigationObservations: true})});
      })();
    )";

    const std::string get_last_lcp_start = R"(
      (async () => {
        const last_lcp_entry = await new Promise(
          resolve => {
            (new PerformanceObserver(
              list => {
                const entries = list.getEntries();
                resolve(entries[entries.length - 1]);
              })).observe({type: 'largest-contentful-paint', buffered: true,
                           includeSoftNavigationObservations: true})});
        return last_lcp_entry.startTime;
      })();
    )";

    // Get the web exposed LCP value before the click.
    int lcp_start_before =
        EvalJs(web_contents, get_last_lcp_start).ExtractDouble();

    content::SimulateMouseClickAt(
        browser()->tab_strip_model()->GetActiveWebContents(), 0,
        blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));

    // Wait for an LCP entry to fire.
    if (wait_for_second_lcp) {
      ASSERT_TRUE(EvalJs(web_contents, wait_for_lcp).error.empty());
    }

    // Get the web exposed LCP value after the click
    int lcp_start_after =
        EvalJs(web_contents, get_last_lcp_start).ExtractDouble();
    ASSERT_GE(lcp_start_after, lcp_start_before);

    // Wait for a soft navigation count update.
    waiter->Wait();

    // Force navigation to another page, which should force logging of
    // histograms persisted at the end of the page load lifetime.
    NavigateToUntrackedUrl();

    VerifyNavigationMetrics({url});
    int64_t soft_navigation_count =
        GetUKMPageLoadMetric(PageLoad::kSoftNavigationCountName);
    ASSERT_EQ(soft_navigation_count, 1);

    auto lcp_value_bucket_start =
        histogram_tester_
            ->GetAllSamples(internal::kHistogramLargestContentfulPaint)[0]
            .min;

    // The histogram value represents the low end of the bucket, not the actual
    // value. Therefore it is lower or equal to the web exposed value.
    ASSERT_LE(lcp_value_bucket_start, lcp_start_before);

    VerifyTraceEvents(StopTracing(), wait_for_second_lcp ? 3UL : 1UL);
  }

 private:
  void StartTracing() {
    base::RunLoop wait_for_tracing;
    content::TracingController::GetInstance()->StartTracing(
        base::trace_event::TraceConfig(
            "{\"included_categories\": [\"devtools.timeline\"]}"),
        wait_for_tracing.QuitClosure());
    wait_for_tracing.Run();
  }

  std::string StopTracing() {
    base::RunLoop wait_for_tracing;
    std::string trace_output;
    content::TracingController::GetInstance()->StopTracing(
        content::TracingController::CreateStringEndpoint(
            base::BindLambdaForTesting(
                [&](std::unique_ptr<std::string> trace_str) {
                  trace_output = std::move(*trace_str);
                  wait_for_tracing.Quit();
                })));
    wait_for_tracing.Run();
    return trace_output;
  }

  void VerifyTraceEvents(const std::string& trace_str,
                         size_t expected_event_number) {
    std::unique_ptr<TraceAnalyzer> analyzer(TraceAnalyzer::Create(trace_str));
    TraceEventVector events;
    auto query =
        Query::EventNameIs("SoftNavigationHeuristics_SoftNavigationDetected") ||
        Query::EventNameIs("largestContentfulPaint::Candidate");
    size_t num_events = analyzer->FindEvents(query, &events);
    EXPECT_EQ(expected_event_number, num_events);

    std::string previous_frame;
    std::string navigation_id;
    double soft_navigation_timestamp = 0.0;
    for (auto* event : events) {
      EXPECT_TRUE(event->HasStringArg("frame"));
      std::string frame = event->GetKnownArgAsString("frame");
      if (!previous_frame.empty()) {
        EXPECT_EQ(frame, previous_frame);
      }
      previous_frame = frame;
      if (event->name == "SoftNavigationHeuristics_SoftNavigationDetected") {
        soft_navigation_timestamp = event->timestamp;
        EXPECT_TRUE(event->HasStringArg("navigationId"));
        navigation_id = event->GetKnownArgAsString("navigationId");
      } else if (soft_navigation_timestamp > 0.0) {
        EXPECT_LE(soft_navigation_timestamp, event->timestamp);
        EXPECT_EQ(event->name, "largestContentfulPaint::Candidate");
        base::Value::Dict data = event->GetKnownArgAsDict("data");
        if (!navigation_id.empty()) {
          EXPECT_EQ(navigation_id, *data.FindString("navigationId"));
        }
      }
    }
    // If we have more than one event, one of them needs to be a soft
    // navigation.
    if (expected_event_number > 1) {
      EXPECT_TRUE(soft_navigation_timestamp > 0);
    }
  }
};

class SoftNavigationBrowserTestWithSoftNavigationHeuristicsFlag
    : public SoftNavigationBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
    features_list_.InitWithFeatures({blink::features::kSoftNavigationHeuristics,
                                     blink::features::kNavigationId},
                                    {});
  }

 private:
  base::test::ScopedFeatureList features_list_;
};

// TODO(crbug.com/341578843): Flaky on many platforms.
IN_PROC_BROWSER_TEST_F(SoftNavigationBrowserTest, DISABLED_SoftNavigation) {
  TestSoftNavigation(/*wait_for_second_lcp=*/false);
}

// TODO(crbug.com/40946340): Flaky on several platforms.
IN_PROC_BROWSER_TEST_F(
    SoftNavigationBrowserTestWithSoftNavigationHeuristicsFlag,
    DISABLED_SoftNavigation) {
  TestSoftNavigation(/*wait_for_second_lcp=*/true);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, InputEventsForOmniboxMatch) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::SendToOmniboxAndSubmit(browser(), url.spec(),
                                        base::TimeTicks::Now());
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToFirstContentfulPaint, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  NavigateToUntrackedUrl();

  VerifyNavigationMetrics({url});
}

// TODO(crbug.com/334416161): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InputEventsForJavaScriptHref DISABLED_InputEventsForJavaScriptHref
#else
#define MAYBE_InputEventsForJavaScriptHref InputEventsForJavaScriptHref
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_InputEventsForJavaScriptHref) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url =
      embedded_test_server()->GetURL("/page_load_metrics/javascript_href.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  waiter->Wait();
  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramInputToNavigation, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramInputToNavigationLinkClick, 1);
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  GURL url = embedded_test_server()->GetURL(
      "/page_load_metrics/javascript_window_open.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/scroll.html")));
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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a page that registers a service worker.
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('fetch_event_pass_through.js');"));
  waiter->Wait();

  // The first load was not controlled, so service worker metrics should not be
  // logged.
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 0);

  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a controlled page.
  GURL controlled_url = url;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), controlled_url));
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

// Does a navigation to a page controlled by a skippable service worker
// fetch handler and verifies that the page load metrics are logged.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       ServiceWorkerSkippableFetchHandlerMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a page that registers a service worker.
  GURL url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('empty_fetch_event.js');"));
  waiter->Wait();

  // The first load was not controlled, so service worker metrics should not be
  // logged.
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 0);

  waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstPaint);

  // Load a controlled page.
  GURL controlled_url = url;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), controlled_url));
  waiter->Wait();

  // The metrics should be logged.
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstPaint, 2);
  histogram_tester_->ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstPaint, 1);
  histogram_tester_->ExpectTotalCount(
      internal::
          kHistogramServiceWorkerFirstContentfulPaintSkippableFetchHandler,
      1);

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

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  waiter->Wait();

  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kSecureContextCheckPassed), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kSecureContextCheckFailed), 0);
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionsMainFrame \
  DISABLED_MainFrameIntersectionsMainFrame
#else
#define MAYBE_MainFrameIntersectionsMainFrame MainFrameIntersectionsMainFrame
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionsMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
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

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionSingleFrame \
  DISABLED_MainFrameIntersectionSingleFrame
#else
#define MAYBE_MainFrameIntersectionSingleFrame MainFrameIntersectionSingleFrame
#endif
// Creates a single frame within the main frame and verifies the intersection
// with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionSingleFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  waiter->AddMainFrameIntersectionExpectation(gfx::Rect(100, 100, 200, 200));

  // Create a 200x200 iframe at 100,100.
  EXPECT_TRUE(ExecJs(web_contents,
                     "createIframeAtRect(\"test\", 100, 100, 200, 200);"));

  waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionSameOrigin \
  DISABLED_MainFrameIntersectionSameOrigin
#else
#define MAYBE_MainFrameIntersectionSameOrigin MainFrameIntersectionSameOrigin
#endif
// Creates a set of nested frames within the main frame and verifies
// their intersections with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionSameOrigin) {
  EXPECT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
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
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(
      ExecJs(child_frame, "createIframeAtRect(\"test2\", 10, 10, 300, 300);"));

  waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionCrossOrigin \
  DISABLED_MainFrameIntersectionCrossOrigin
#else
#define MAYBE_MainFrameIntersectionCrossOrigin MainFrameIntersectionCrossOrigin
#endif
// Creates a set of nested frames, with a cross origin subframe, within the
// main frame and verifies their intersections with the main frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionCrossOrigin) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
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
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(
      ExecJs(child_frame, "createIframeAtRect(\"test2\", 10, 10, 300, 300);"));

  waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionCrossOriginOutOfView \
  DISABLED_MainFrameIntersectionCrossOriginOutOfView
#else
#define MAYBE_MainFrameIntersectionCrossOriginOutOfView \
  MainFrameIntersectionCrossOriginOutOfView
#endif
// Creates a set of nested frames, with a cross origin subframe that is out of
// view within the main frame and verifies their intersections with the main
// frame.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionCrossOriginOutOfView) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
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
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(ExecJs(child_frame,
                     "createIframeAtRect(\"test2\", 5000, 5000, 190, 190);"));

  waiter->Wait();
}

// TODO(crbug.com/40916877): Fix this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MainFrameIntersectionCrossOriginScrolled \
  DISABLED_MainFrameIntersectionCrossOriginScrolled
#else
#define MAYBE_MainFrameIntersectionCrossOriginScrolled \
  MainFrameIntersectionCrossOriginScrolled
#endif
// Creates a set of nested frames, with a cross origin subframe that is out of
// view within the main frame and verifies their intersections with the main
// frame. The out of view frame is then scrolled back into view and the
// intersection is verified.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_MainFrameIntersectionCrossOriginScrolled) {
  EXPECT_TRUE(embedded_test_server()->Start());
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/page_load_metrics/blank_with_positioned_iframe_writer.html")));
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
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
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
  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter->AddPageExpectation(TimingField::kLargestContentfulPaint);

  //  Waiter to ensure that iframe content is loaded.
  auto waiter2 = CreatePageLoadMetricsTestWaiter("waiter2");
  waiter2->AddPageExpectation(TimingField::kLoadEvent);
  waiter2->AddSubFrameExpectation(TimingField::kLoadEvent);
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddSubFrameExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kLargestContentfulPaint);
  waiter2->AddSubFrameExpectation(TimingField::kLargestContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kFirstInputOrScroll);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/page_load_metrics/click_to_create_iframe.html")));
  waiter->Wait();

  // Tap in the middle of the button.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "button");
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
// The LinkPreview feature is implemented only on desktops, and window
// implementation assumes the Aura for now.
// TODO(crbug.com/305004651): Implement the feature for other platforms and
// enable the following tests on the remaining platforms.
class PageLoadMetricsPreviewBrowserTest : public PageLoadMetricsBrowserTest {
 public:
  PageLoadMetricsPreviewBrowserTest() {
    helper_ = std::make_unique<test::PreviewTestHelper>(
        base::BindRepeating(&PageLoadMetricsPreviewBrowserTest::web_contents,
                            base::Unretained(this)));
  }

 protected:
  std::unique_ptr<test::PreviewTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(PageLoadMetricsPreviewBrowserTest,
                       PreviewPrimaryPageType) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTrackerPageType,
      page_load_metrics::internal::PageLoadTrackerPageType::kPrimaryPage, 1);

  helper_->InitiatePreview(embedded_test_server()->GetURL("/title2.html"));
  helper_->WaitUntilLoadFinished();

  histogram_tester_->ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTrackerPageType,
      page_load_metrics::internal::PageLoadTrackerPageType::kPreviewPrimaryPage,
      1);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)

class PageLoadMetricsBrowserTestTerminatedPage
    : public PageLoadMetricsBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    PageLoadMetricsBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 public:
  content::WebContents* OpenTabAndNavigate() {
    content::OpenURLParams page(embedded_test_server()->GetURL("/title1.html"),
                                content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);

    content::WebContents* contents =
        browser()->OpenURL(page, /*navigation_handle_callback=*/{});
    std::unique_ptr<PageLoadMetricsTestWaiter> waiter =
        CreatePageLoadMetricsTestWaiter("lcp_waiter", contents);
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kLargestContentfulPaint);

    content::TestNavigationObserver observer(contents);
    observer.set_expected_initial_url(page.url);
    observer.Wait();

    // This is to wait for LCP to be observed on browser side.
    waiter->Wait();

    return contents;
  }

  double GetLCPTimeFromEmittedLCPEntry(content::WebContents* contents) {
    content::EvalJsResult lcp_time =
        EvalJs(contents, ScriptForGettingLCPTimeFromEmittedLCPEntry());
    return lcp_time.ExtractDouble();
  }

  void AddNewTab() {
    std::unique_ptr<content::WebContents> web_contents_to_add =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));

    web_contents_to_add->GetController().LoadURL(
        embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
        ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

    auto* tab_strip_model = browser()->tab_strip_model();
    tab_strip_model->AddWebContents(std::move(web_contents_to_add), -1,
                                    ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    AddTabTypes::ADD_ACTIVE);
  }

  void DiscardTab(content::WebContents* contents) {
    resource_coordinator::TabLifecycleUnitExternal::FromWebContents(contents)
        ->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);
  }

  void CloseTab(content::WebContents* contents) {
    auto* tab_strip_model = browser()->tab_strip_model();
    // Get the total count of tabs.
    int tab_count = tab_strip_model->count();

    // Get the tab index of the given WebContents.
    int tab_index = tab_strip_model->GetIndexOfWebContents(contents);
    // Expect the tab index of the given WebContents is found.
    EXPECT_NE(tab_index, TabStripModel::kNoTab);

    // Close the tab corresponding to the given WebContents.
    tab_strip_model->CloseWebContentsAt(tab_index,
                                        TabCloseTypes::CLOSE_USER_GESTURE);
    // Verify tab is closed.
    EXPECT_EQ(tab_strip_model->count(), tab_count - 1);
  }

  std::string ScriptForGettingLCPTimeFromEmittedLCPEntry() {
    return R"(
   (async () => {
        return await new Promise(resolve => {
          (new PerformanceObserver(list => {
              const entries = list.getEntries();
              for (let entry of entries) {
                  if (entry) {
                      resolve(entry.startTime);
                  }
              }
          }))
          .observe({
              type: 'largest-contentful-paint',
              buffered: true
          });
      })
  })())";
  }
};

class PageLoadMetricsBrowserTestDiscardedPage
    : public PageLoadMetricsBrowserTestTerminatedPage,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(PageLoadMetricsBrowserTestDiscardedPage,
                       UkmIsRecordedForDiscardedTabPage) {
  if (base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    GTEST_SKIP() << "Page load metrics are reported when the tab is closed.";
  }

  // Open a new foreground tab and navigate.
  content::WebContents* contents = OpenTabAndNavigate();

  // Wait for LCP emission and observation.
  double lcp_time = GetLCPTimeFromEmittedLCPEntry(contents);

  // Background current tab by adding a new tab if provided param is true.
  if (GetParam()) {
    // Add a new tab.
    AddNewTab();

    // Verify the first tab is backgrounded.
    EXPECT_NE(contents, browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Discard tab.
  DiscardTab(contents);

  // Verify tab is discarded.
  EXPECT_TRUE(
      browser()->tab_strip_model()->GetWebContentsAt(1)->WasDiscarded());

  // Verify page load metric is recorded.
  EXPECT_NEAR(
      GetUKMPageLoadMetric(
          PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name),
      lcp_time, 10);
}

INSTANTIATE_TEST_SUITE_P(DiscardedPages,
                         PageLoadMetricsBrowserTestDiscardedPage,
                         testing::Bool());

class PageLoadMetricsBrowserTestClosedPage
    : public PageLoadMetricsBrowserTestTerminatedPage,
      public ::testing::WithParamInterface<bool> {};

IN_PROC_BROWSER_TEST_P(PageLoadMetricsBrowserTestClosedPage,
                       UkmIsRecordedForClosedTabPage) {
  // Open a new foreground tab and navigate. The new tab would be of index 1
  // which would be used below in verifying the tab is discarded.
  content::WebContents* contents = OpenTabAndNavigate();

  // Wait for LCP emission and observation. This is to ensure there is an LCP
  // entry to report at the time of closing the page.
  double lcp_time = GetLCPTimeFromEmittedLCPEntry(contents);

  // Background current tab by adding a new tab if provided param is true.
  if (GetParam()) {
    // Add a new tab.
    AddNewTab();

    // Verify the tab is backgrounded.
    EXPECT_NE(contents, browser()->tab_strip_model()->GetActiveWebContents());
  }

  // close tab.
  CloseTab(contents);

  // Verify page load metric is recorded.
  EXPECT_NEAR(
      GetUKMPageLoadMetric(
          PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name),
      lcp_time, 10);
}

INSTANTIATE_TEST_SUITE_P(ClosedPages,
                         PageLoadMetricsBrowserTestClosedPage,
                         testing::Bool());

// This test is to verify page load metrics are recorded in case when the
// render process is shut down.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestTerminatedPage,
                       UkmIsRecordedWhenRenderProcessShutsDown) {
  content::WebContents* contents = OpenTabAndNavigate();

  // Wait for LCP emission and observation.
  double lcp_time = GetLCPTimeFromEmittedLCPEntry(contents);
  content::RenderProcessHost* process = RenderFrameHost()->GetProcess();

  // Shut down render process.
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(process->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer.Wait();
  EXPECT_FALSE(RenderFrameHost()->IsRenderFrameLive());

  // Verify page load metric is recorded.
  EXPECT_NEAR(
      GetUKMPageLoadMetric(
          PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name),
      lcp_time, 10);
}

// This class is used to verify page load metrics are recorded in case of
// crashes of different kinds. These crashes are simulated by navigating to the
// chrome debug urls.
class PageLoadMetricsBrowserTestCrashedPage
    : public PageLoadMetricsBrowserTestTerminatedPage,
      public ::testing::WithParamInterface<const char*> {};

// TODO(crbug.com/40280758): Very flaky on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_UkmIsRecordedForCrashedTabPage \
  DISABLED_UkmIsRecordedForCrashedTabPage
#else
#define MAYBE_UkmIsRecordedForCrashedTabPage UkmIsRecordedForCrashedTabPage
#endif
IN_PROC_BROWSER_TEST_P(PageLoadMetricsBrowserTestCrashedPage,
                       MAYBE_UkmIsRecordedForCrashedTabPage) {
  // Open a new foreground tab and navigate.
  content::WebContents* contents = OpenTabAndNavigate();

  // The back/forward cache is disabled because page load metrics can also be
  // recorded when entering into the bfcache. We want to test that page load
  // metrics are recorded via the PageLoadTracker destructor which is called in
  // all crash cases.
  content::DisableBackForwardCacheForTesting(
      contents, content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Wait for LCP emission and observation. This is to ensure there is an LCP
  // entry to report at the time of killing the page.
  double lcp_time = GetLCPTimeFromEmittedLCPEntry(contents);

  // Kill the page.
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(GetParam())));

  // Page being crashed is only verifiable in these crashes.
  if (GetParam() == blink::kChromeUIKillURL ||
      GetParam() == blink::kChromeUICrashURL)
    EXPECT_TRUE(
        browser()->tab_strip_model()->GetActiveWebContents()->IsCrashed());

  // Verify page load metric is recorded.
  EXPECT_NEAR(
      GetUKMPageLoadMetric(
          PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name),
      lcp_time, 10);
}

INSTANTIATE_TEST_SUITE_P(
    CrashCases,
    PageLoadMetricsBrowserTestCrashedPage,
    testing::ValuesIn({blink::kChromeUIKillURL, blink::kChromeUICrashURL,
                       blink::kChromeUIGpuCrashURL,
                       blink::kChromeUIBrowserCrashURL,
                       blink::kChromeUINetworkErrorURL,
                       blink::kChromeUIProcessInternalsURL}));

// Test is flaky. https://crbug.com/1260953
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_WIN)
#define MAYBE_PageLCPAnimatedImage DISABLED_PageLCPAnimatedImage
#else
#define MAYBE_PageLCPAnimatedImage PageLCPAnimatedImage
#endif
// Tests that an animated image's reported LCP values are smaller than its load
// times, when the feature flag for animated image reporting is enabled.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestAnimatedLCP,
                       MAYBE_PageLCPAnimatedImage) {
  test_animated_image_lcp(/*smaller=*/true, /*animated=*/true);
}

// Tests that a non-animated image's reported LCP values are larger than its
// load times, when the feature flag for animated image reporting is enabled.
// TODO(crbug.com/40218474): Flaky on Mac/Linux/Lacros.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PageLCPNonAnimatedImage DISABLED_PageLCPNonAnimatedImage
#else
#define MAYBE_PageLCPNonAnimatedImage PageLCPNonAnimatedImage
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestAnimatedLCP,
                       MAYBE_PageLCPNonAnimatedImage) {
  test_animated_image_lcp(/*smaller=*/false, /*animated=*/false);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, FirstInputDelayFromClick) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kLoadEvent);
  waiter->AddPageExpectation(TimingField::kFirstContentfulPaint);

  auto waiter2 = CreatePageLoadMetricsTestWaiter("waiter2");
  waiter2->AddPageExpectation(TimingField::kLoadEvent);
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter2->AddPageExpectation(TimingField::kFirstInputDelay);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/page_load_metrics/click.html")));
  waiter->Wait();
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  waiter2->Wait();

  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 1);
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputTimestamp,
                                      1);
}

IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest, SameOriginNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL kUrl1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL kUrl2 = embedded_test_server()->GetURL("a.com", "/title2.html");

  auto waiter1 = CreatePageLoadMetricsTestWaiter("waiter1");
  waiter1->AddPageExpectation(TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl1));
  waiter1->Wait();

  auto waiter2 = CreatePageLoadMetricsTestWaiter("waiter2");
  waiter2->AddPageExpectation(TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl2));
  waiter2->Wait();

  NavigateToUntrackedUrl();
  VerifyNavigationMetrics({kUrl1, kUrl2});

  // Navigation from about:blank to kUrl1 is a cross origin navigation.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.CrossOrigin.FirstContentfulPaint", 1);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.CrossOrigin.LargestContentfulPaint", 1);
  // Navigation from kUrl1 to kUrl2 is a same origin navigation.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.SameOrigin.FirstContentfulPaint", 1);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.SameOrigin.LargestContentfulPaint", 1);
}

// TODO(crbug.com/334416161): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CrossOriginNavigation DISABLED_CrossOriginNavigation
#else
#define MAYBE_CrossOriginNavigation CrossOriginNavigation
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTest,
                       MAYBE_CrossOriginNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL kUrl1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL kUrl2 = embedded_test_server()->GetURL("b.com", "/title1.html");

  auto waiter1 = CreatePageLoadMetricsTestWaiter("waiter1");
  waiter1->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter1->AddPageExpectation(TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl1));
  waiter1->Wait();

  auto waiter2 = CreatePageLoadMetricsTestWaiter("waiter2");
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  waiter1->AddPageExpectation(TimingField::kLargestContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl2));
  waiter2->Wait();

  NavigateToUntrackedUrl();
  VerifyNavigationMetrics({kUrl1, kUrl2});

  // Navigation from about:blank to kUrl1 and navigation from kUrl1 to kUrl2 are
  // cross origin navigations.
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.CrossOrigin.FirstContentfulPaint", 2);
  histogram_tester_->ExpectTotalCount(
      "PageLoad.Clients.CrossOrigin.LargestContentfulPaint", 2);
}

class PageLoadMetricsBrowserTestWithFencedFrames
    : public PageLoadMetricsBrowserTest {
 public:
  PageLoadMetricsBrowserTestWithFencedFrames()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }
  ~PageLoadMetricsBrowserTestWithFencedFrames() override = default;

 protected:
  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  content::test::FencedFrameTestHelper helper_;
};

// TODO(crbug.com/334416161): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PageLoadPrivacySandboxAdsFencedFramesMetrics \
  DISABLED_PageLoadPrivacySandboxAdsFencedFramesMetrics
#else
#define MAYBE_PageLoadPrivacySandboxAdsFencedFramesMetrics \
  PageLoadPrivacySandboxAdsFencedFramesMetrics
#endif
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithFencedFrames,
                       MAYBE_PageLoadPrivacySandboxAdsFencedFramesMetrics) {
  ASSERT_TRUE(https_server().Start());

  static constexpr char
      kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaint[] =
          "PageLoad.Clients.PrivacySandboxAds.PaintTiming."
          "NavigationToFirstContentfulPaint.FencedFrames";

  // Not recorded as fenced frame is not created.
  auto waiter1 = CreatePageLoadMetricsTestWaiter("waiter1");
  waiter1->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server().GetURL("a.test", "/title1.html")));
  waiter1->Wait();

  histogram_tester_->ExpectTotalCount(
      kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaint, 0);

  // Recorded as fenced frame is created.
  auto waiter2 = CreatePageLoadMetricsTestWaiter("waiter2");
  waiter2->AddPageExpectation(TimingField::kFirstContentfulPaint);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server().GetURL("c.test", "/fenced_frames/basic_title.html")));
  waiter2->Wait();

  histogram_tester_->ExpectTotalCount(
      kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaint, 1);
}

class PageLoadMetricsBrowserTestWithBackForwardCache
    : public PageLoadMetricsBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kEnterBackForwardCache, 0);
  histogram_tester_->ExpectBucketCount(
      internal::kHistogramBackForwardCacheEvent,
      internal::PageLoadBackForwardCacheEvent::kRestoreFromBackForwardCache, 0);

  // Go to URL2. The previous page (URL1) is put into the back-forward cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

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

// Test UseCounter UKM features observed when a page is in the BFCache and is
// evicted from it.
IN_PROC_BROWSER_TEST_F(PageLoadMetricsBrowserTestWithBackForwardCache,
                       UseCounterUkmFeaturesLoggedOnBFCacheEviction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/page_load_metrics/use_counter_features.html");
  {
    auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
    waiter->AddPageExpectation(TimingField::kLoadEvent);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    MakeComponentFullscreen("testvideo");
    waiter->Wait();
  }
  NavigateToUntrackedUrl();

  // Force the BFCache to evict all entries. This should cause the
  // UseCounter histograms to be logged.
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .GetBackForwardCache()
      .Flush();

  // Navigate to a new URL. This gives the various page load tracking
  // mechanisms time to process the BFCache evictions.
  auto url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Blink_UseCounter::kEntryName);
  EXPECT_THAT(entries, SizeIs(4));
  std::vector<int64_t> ukm_features;
  for (const ukm::mojom::UkmEntry* entry : entries) {
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
                  static_cast<int64_t>(WebFeature::kPageVisits)));

  // Check histogram counts.
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kPageVisits), 2);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kFullscreenSecureOrigin), 1);
  histogram_tester_->ExpectBucketCount(
      "Blink.UseCounter.Features",
      static_cast<int32_t>(WebFeature::kNavigatorVibrate), 1);
}

class NavigationPageLoadMetricsBrowserTest
    : public PageLoadMetricsBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  NavigationPageLoadMetricsBrowserTest() = default;
  ~NavigationPageLoadMetricsBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/40188113): This test used an experiment param (which no
    // longer exists) to suppress the metrics send timer. If and when the test
    // is re-enabled, it should be updated to use a different mechanism.
    PageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_P(NavigationPageLoadMetricsBrowserTest, FirstInputDelay) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL(
      (GetParam() == "SameSite") ? "a.com" : "b.com", "/title2.html"));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  internal::kHistogramFirstContentfulPaint),
              testing::IsEmpty());

  auto waiter = CreatePageLoadMetricsTestWaiter("waiter");
  waiter->AddPageExpectation(TimingField::kFirstInputDelay);

  // 1) Navigate to url1.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url1));
  histogram_tester_->ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);
  content::RenderFrameHost* rfh_a = RenderFrameHost();
  content::RenderProcessHost* rfh_a_process = rfh_a->GetProcess();

  // We should wait for the main frame's hit-test data to be ready before
  // sending the click event below to avoid flakiness.
  content::WaitForHitTestData(web_contents()->GetPrimaryMainFrame());
  // Ensure the compositor thread is ready for mouse events.
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();

  // Simulate mouse click. FirstInputDelay won't get updated immediately.
  content::SimulateMouseClickAt(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(100, 100));

  // Run a Performance Observer to ensure the renderer receives the click
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
          (async () => {
            await new Promise(resolve => {
              new PerformanceObserver(e => {
                e.getEntries().forEach(entry => {
                  resolve(true);
                })
              }).observe({type: 'first-input', buffered: true});
          })})())"));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();

  waiter->Wait();
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
    // TODO(crbug.com/40157795): Support updating metrics consistently on
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

class PrerenderPageLoadMetricsBrowserTest : public PageLoadMetricsBrowserTest {
 public:
  PrerenderPageLoadMetricsBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderPageLoadMetricsBrowserTest::web_contents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    PageLoadMetricsBrowserTest::SetUp();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsBrowserTest, PrerenderEvent) {
  using page_load_metrics::internal::kPageLoadPrerender2Event;
  using page_load_metrics::internal::PageLoadPrerenderEvent;

  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kNavigationInPrerenderedMainFrame, 0);
  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kPrerenderActivationNavigation, 0);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);

  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kNavigationInPrerenderedMainFrame, 1);
  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kPrerenderActivationNavigation, 0);

  // Activate.
  prerender_helper_.NavigatePrimaryPage(prerender_url);

  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kNavigationInPrerenderedMainFrame, 1);
  histogram_tester_->ExpectBucketCount(
      kPageLoadPrerender2Event,
      PageLoadPrerenderEvent::kPrerenderActivationNavigation, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsBrowserTest,
                       PrerenderingDoNotRecordUKM) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  content::FrameTreeNodeId host_id =
      prerender_helper_.AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());
  auto entries =
      test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Activate.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  entries = test_ukm_recorder_->GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

enum BackForwardCacheStatus { kDisabled = 0, kEnabled = 1 };

class PageLoadMetricsBackForwardCacheBrowserTest
    : public PageLoadMetricsBrowserTest,
      public testing::WithParamInterface<BackForwardCacheStatus> {
 public:
  PageLoadMetricsBackForwardCacheBrowserTest() {
    if (GetParam() == BackForwardCacheStatus::kEnabled) {
      // Enable BackForwardCache.
      feature_list_.InitWithFeaturesAndParameters(
          content::GetBasicBackForwardCacheFeatureForTesting(),
          content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      feature_list_.InitAndDisableFeature(features::kBackForwardCache);
      DCHECK(!content::BackForwardCache::IsBackForwardCacheFeatureEnabled());
    }
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "BFCacheEnabled" : "BFCacheDisabled";
  }

  void VerifyPageEndReasons(const std::vector<PageEndReason>& reasons,
                            const GURL& url,
                            bool is_bfcache_enabled);
  int64_t CountForMetricForURL(std::string_view entry_name,
                               std::string_view metric_name,
                               const GURL& url);
  void ExpectNewForegroundDuration(const GURL& url, bool expect_bfcache);

 private:
  int64_t expected_page_load_foreground_durations_ = 0;
  int64_t expected_bfcache_foreground_durations_ = 0;

  base::test::ScopedFeatureList feature_list_;
};

// Verifies the page end reasons are as we expect. This means that the first
// page end reason is always recorded in Navigation.PageEndReason3, and
// subsequent reasons are recorded in HistoryNavigation.PageEndReason if bfcache
// is enabled, or Navigation.PageEndReason3 if not.
void PageLoadMetricsBackForwardCacheBrowserTest::VerifyPageEndReasons(
    const std::vector<PageEndReason>& reasons,
    const GURL& url,
    bool is_bfcache_enabled) {
  unsigned int reason_index = 0;
  for (const ukm::mojom::UkmEntry* entry :
       test_ukm_recorder_->GetEntriesByName(PageLoad::kEntryName)) {
    auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
    if (source->url() != url)
      continue;
    if (test_ukm_recorder_->EntryHasMetric(
            entry, PageLoad::kNavigation_PageEndReason3Name)) {
      if (is_bfcache_enabled) {
        // If bfcache is on then only one of these should exist, so the index
        // should be zero.
        EXPECT_EQ(reason_index, 0U);
      }
      ASSERT_LT(reason_index, reasons.size());
      test_ukm_recorder_->ExpectEntryMetric(
          entry, PageLoad::kNavigation_PageEndReason3Name,
          reasons[reason_index++]);
    }
  }
  if (is_bfcache_enabled) {
    EXPECT_EQ(reason_index, 1U);
  } else {
    EXPECT_EQ(reason_index, reasons.size());
  }
  for (const ukm::mojom::UkmEntry* entry :
       test_ukm_recorder_->GetEntriesByName(HistoryNavigation::kEntryName)) {
    auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
    if (source->url() != url)
      continue;
    if (test_ukm_recorder_->EntryHasMetric(
            entry, HistoryNavigation::
                       kPageEndReasonAfterBackForwardCacheRestoreName)) {
      EXPECT_TRUE(is_bfcache_enabled);
      ASSERT_LT(reason_index, reasons.size());
      test_ukm_recorder_->ExpectEntryMetric(
          entry,
          HistoryNavigation::kPageEndReasonAfterBackForwardCacheRestoreName,
          reasons[reason_index++]);
    }
  }
  // Should have been through all the reasons.
  EXPECT_EQ(reason_index, reasons.size());
}

int64_t PageLoadMetricsBackForwardCacheBrowserTest::CountForMetricForURL(
    std::string_view entry_name,
    std::string_view metric_name,
    const GURL& url) {
  int64_t count = 0;
  for (const ukm::mojom::UkmEntry* entry :
       test_ukm_recorder_->GetEntriesByName(entry_name)) {
    auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
    if (source->url() != url)
      continue;
    if (test_ukm_recorder_->EntryHasMetric(entry, metric_name)) {
      count++;
    }
  }
  return count;
}

IN_PROC_BROWSER_TEST_P(PageLoadMetricsBackForwardCacheBrowserTest,
                       LogsPageEndReasons) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  bool back_forward_cache_enabled = GetParam() == kEnabled;
  // Navigate to A.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());

  // Navigate to B.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  if (back_forward_cache_enabled) {
    ASSERT_EQ(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  std::vector<PageEndReason> expected_reasons_a;
  expected_reasons_a.push_back(page_load_metrics::END_NEW_NAVIGATION);
  VerifyPageEndReasons(expected_reasons_a, url_a, back_forward_cache_enabled);

  // Go back to A, restoring it from the back-forward cache (if enabled)
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate to B again - this should trigger the
  // BackForwardCachePageLoadMetricsObserver for A (if enabled)
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  expected_reasons_a.push_back(page_load_metrics::END_NEW_NAVIGATION);
  VerifyPageEndReasons(expected_reasons_a, url_a, back_forward_cache_enabled);

  // Go back to A, restoring it from the back-forward cache (again)
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate to B using GoForward() to verify the correct page end reason
  // is stored for A.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  expected_reasons_a.push_back(page_load_metrics::END_FORWARD_BACK);
  VerifyPageEndReasons(expected_reasons_a, url_a, back_forward_cache_enabled);
}

void PageLoadMetricsBackForwardCacheBrowserTest::ExpectNewForegroundDuration(
    const GURL& url,
    bool expect_bfcache) {
  if (expect_bfcache) {
    expected_bfcache_foreground_durations_++;
  } else {
    expected_page_load_foreground_durations_++;
  }
  int64_t bf_count = CountForMetricForURL(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName,
      url);
  int64_t pl_count = CountForMetricForURL(
      PageLoad::kEntryName, PageLoad::kPageTiming_ForegroundDurationName, url);
  EXPECT_EQ(bf_count, expected_bfcache_foreground_durations_);
  EXPECT_EQ(pl_count, expected_page_load_foreground_durations_);
}

IN_PROC_BROWSER_TEST_P(PageLoadMetricsBackForwardCacheBrowserTest,
                       LogsBasicPageForegroundDuration) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  bool back_forward_cache_enabled = GetParam() == kEnabled;
  // Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());

  // Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  if (back_forward_cache_enabled) {
    ASSERT_EQ(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  // Verify a new foreground duration - this one shouldn't be logged by the
  // bfcache metrics regardless of bfcache being enabled or not.
  ExpectNewForegroundDuration(url_a, /*expect_bfcache=*/false);

  // Go back to A, restoring it from the back-forward cache (if enabled)
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Navigate to B again - this should trigger the
  // BackForwardCachePageLoadMetricsObserver for A (if enabled)
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  ExpectNewForegroundDuration(url_a, back_forward_cache_enabled);

  // Go back to A, restoring it from the back-forward cache (again)
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  web_contents()->GetController().GoForward();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Verify another foreground duration was logged.
  ExpectNewForegroundDuration(url_a, back_forward_cache_enabled);
}

IN_PROC_BROWSER_TEST_P(PageLoadMetricsBackForwardCacheBrowserTest,
                       LogsPageForegroundDurationOnHide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  bool back_forward_cache_enabled = GetParam() == kEnabled;
  // Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper rfh_a(web_contents()->GetPrimaryMainFrame());

  // Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  if (back_forward_cache_enabled) {
    ASSERT_EQ(rfh_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  // Verify a new foreground duration - this one shouldn't be logged by the
  // bfcache metrics regardless of bfcache being enabled or not.
  ExpectNewForegroundDuration(url_a, /*expect_bfcache=*/false);

  // Go back to A, restoring it from the back-forward cache (if enabled)
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Open and move to a new tab. This hides A, which should log a foreground
  // duration.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_b, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // The new tab opening should cause a foreground duration for the original
  // tab, since it's been hidden.
  ExpectNewForegroundDuration(url_a, back_forward_cache_enabled);

  // From this point no more foreground durations are expected to be logged, so
  // stash the current counts.
  int64_t bf_count = CountForMetricForURL(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName,
      url_a);
  int64_t pl_count =
      CountForMetricForURL(PageLoad::kEntryName,
                           PageLoad::kPageTiming_ForegroundDurationName, url_a);

  // Switch back to the tab for url_a.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_a, WindowOpenDisposition::SWITCH_TO_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  // And then switch back to url_b's tab. This should call OnHidden for the
  // url_a tab again, but no new foreground duration should be logged.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_b, WindowOpenDisposition::SWITCH_TO_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  int64_t bf_count_after_switch = CountForMetricForURL(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName,
      url_a);
  int64_t pl_count_after_switch =
      CountForMetricForURL(PageLoad::kEntryName,
                           PageLoad::kPageTiming_ForegroundDurationName, url_a);
  EXPECT_EQ(bf_count, bf_count_after_switch);
  EXPECT_EQ(pl_count, pl_count_after_switch);

  // Switch back to the tab for url_a, then close the browser. This should cause
  // OnComplete to be called on the BFCache observer, but this should not cause
  // a new foreground duration to be logged.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url_a, WindowOpenDisposition::SWITCH_TO_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  CloseBrowserSynchronously(browser());

  // Neither of the metrics for url_a should have moved.
  int64_t bf_count_after_close = CountForMetricForURL(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName,
      url_a);
  int64_t pl_count_after_close =
      CountForMetricForURL(PageLoad::kEntryName,
                           PageLoad::kPageTiming_ForegroundDurationName, url_a);
  EXPECT_EQ(bf_count, bf_count_after_close);
  EXPECT_EQ(pl_count, pl_count_after_close);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PageLoadMetricsBackForwardCacheBrowserTest,
    testing::ValuesIn({BackForwardCacheStatus::kDisabled,
                       BackForwardCacheStatus::kEnabled}),
    PageLoadMetricsBackForwardCacheBrowserTest::DescribeParams);

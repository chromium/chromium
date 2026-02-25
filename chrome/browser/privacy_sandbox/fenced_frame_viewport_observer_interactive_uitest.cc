// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using content::BackForwardCache;
using content::JsReplace;
using content::RenderFrameDeletedObserver;
using content::RenderFrameHost;
using content::TestFrameNavigationObserver;
using content::WebContents;
using content::test::FencedFrameTestHelper;
using net::HostResolver;

using FencedFrameVisibilityObserver =
    FencedFrameTestHelper::FencedFrameVisibilityObserver;

// Tests for FencedFrameViewportObserver, which is responsible for tracking how
// many same-site fenced frames are in the viewport across the life of each
// primary page load. The observer class is never directly referenced in these
// tests, because its creation and destruction are managed by WebContentsImpl.
// Run as an InteractiveBrowserTest to prevent multiple parallel tests on
// builders from messing with the viewport dimensions.
class FencedFrameViewportObserverBrowserTest : public InteractiveBrowserTest {
 public:
  FencedFrameViewportObserverBrowserTest()
      : https_server_(net::EmbeddedTestServer::Type::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  ~FencedFrameViewportObserverBrowserTest() override {
    // Shutdown the server explicitly so that there is no race with the
    // destruction of cookie_headers_map_ and invocation of RequestMonitor.
    if (https_server()->Started()) {
      EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    }
  }

  void SetUpOnMainThread() final {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());

    ASSERT_TRUE(https_server()->Start());
  }

  WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  RenderFrameHost* primary_main_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  // The `overall_max` is the max number of same-site frames in the viewport
  // across the life of the top-level page load. The `unload_time_max` is the
  // max number of same-site frames in the viewport at the moment the top-level
  // page is navigated away or destroyed.
  void ValidateHistograms(int overall_max, int unload_time_max) {
    content::FetchHistogramsFromChildProcesses();
    histogram_tester_.ExpectTotalCount(
        blink::kMaxSameSiteFencedFramesInViewportPerPageLoad, 1);
    histogram_tester_.ExpectBucketCount(
        blink::kMaxSameSiteFencedFramesInViewportPerPageLoad, overall_max, 1);

    histogram_tester_.ExpectTotalCount(
        blink::kMaxSameSiteFencedFramesInViewportAtUnload, 1);
    histogram_tester_.ExpectBucketCount(
        blink::kMaxSameSiteFencedFramesInViewportAtUnload, unload_time_max, 1);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  FencedFrameTestHelper& ff_helper() { return fenced_frame_test_helper_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  net::EmbeddedTestServer https_server_;
  FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       LogVisibleFrameMetricsOnNavigation) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // This navigation will either cause the outermost main frame's Page to enter
  // BFCache, or to be destroyed if BFCache is disabled. Either way, the metrics
  // should be logged by the time the load finishes.
  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. When the primary main frame navigated away, our two
  // same-site fenced frames were still in the viewport.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       LogVisibleFrameMetricsCloseWebContents) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);

  // Close the WebContents. This should cause all child frames to be torn down,
  // snapshotting the metric before doing so.
  browser()->tab_strip_model()->CloseAllTabs();
  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. When the primary main frame was destroyed by the
  // WebContents shutting down, our 2 same-site fenced frames were still in
  // the viewport.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       LogVisibleFrameMetricsCrossSite) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url_1(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  const GURL fenced_frame_url_2(
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
      primary_rfh, fenced_frame_url_1);
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
      primary_rfh, fenced_frame_url_2);

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 1. When the primary main frame navigated away, our two
  // cross-site fenced frames were still in the viewport, so the highest number
  // of same-site frames at unload time was still 1.
  ValidateHistograms(/*overall_max=*/1, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       LogVisibleFrameMetricsNestedFrames) {
  // Create 3 same-site fenced frames, and one same-site iframe.
  const GURL main_url =
      https_server()->GetURL("a.test",
                             "/cross_site_iframe_factory.html?a.test(a.test{"
                             "fenced}(a.test{fenced}),a.test(a.test{fenced}))");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 3:
  // One a.test fenced frame at the top level
  // One a.test fenced frame nested in the top level a.test fenced frame
  // One a.test fenced frame nested in an a.test iframe.
  // Because all of these fenced frames share the same WebContents, no matter
  // how the frames are nested, they should all contribute to the metrics. Also,
  // iframes do not count towards the limit, only fenced frames, so the count
  // should be 3, not 4.
  // When the primary main frame navigated away, our 3 a.test fenced frames were
  // still in the viewport, so the highest number of same-site frames at unload
  // time was still 3.
  ValidateHistograms(/*overall_max=*/3, /*unload_time_max=*/3);
}

// TODO(crbug.com/445735972): Fix failing test on Mac
// TODO(crbug.com/465059974): and Linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_LogVisibleFrameMetricsNestedCrossSiteFrames \
  DISABLED_LogVisibleFrameMetricsNestedCrossSiteFrames
#else
#define MAYBE_LogVisibleFrameMetricsNestedCrossSiteFrames \
  LogVisibleFrameMetricsNestedCrossSiteFrames
#endif
IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       MAYBE_LogVisibleFrameMetricsNestedCrossSiteFrames) {
  const GURL main_url =
      https_server()->GetURL("a.test",
                             "/cross_site_iframe_factory.html?a.test(a.test{"
                             "fenced}(b.test{fenced}),a.test(b.test{fenced}))");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2:
  // One b.test fenced frame nested in an a.test fenced frame
  // One b.test fenced frame nested in an a.test iframe
  // When the primary main frame navigated away, our 2 b.test fenced frames were
  // still in the viewport, so the highest number of same-site frames at unload
  // time was still 2.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       LogVisibleFrameMetricsNestedIframes) {
  // Create some same-site fenced frames with nested iframe children.
  const GURL main_url =
      https_server()->GetURL("a.test",
                             "/cross_site_iframe_factory.html?a.test(a.test{"
                             "fenced}(a.test,a.test),a.test{fenced}(b.test,b."
                             "test,b.test,b.test,b.test))");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2, because iframes don't count towards any metrics. When the
  // primary main frame navigated away, those 2 fenced frames were still in
  // the viewport.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       VisibilityHidden) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);
  // Set one of the fenced frames to 'visibility: hidden' and wait for it to
  // leave the viewport.
  FencedFrameVisibilityObserver visibility_hidden_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kNotRendered, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.style.visibility = 'hidden';
             )"));
  visibility_hidden_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. However, one of the frames was hidden before the
  // navigation, so the total count of visible frames at unload time was 1.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest, DisplayNone) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);
  // Set one of the fenced frames to 'display: none' and wait for it to leave
  // the viewport.
  FencedFrameVisibilityObserver display_none_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kNotRendered, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.style.display = 'none';
             )"));
  display_none_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. However, one of the frames was hidden before the
  // navigation, so the total count of visible frames at unload time was 1.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest, SizeZero) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);
  // Set one of the fenced frames to 'width: 0px' and wait for it to leave
  // the viewport.
  FencedFrameVisibilityObserver size_zero_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kRenderedOutOfViewport, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.style.width = '0px';
             )"));
  size_zero_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. However, one of the frames was hidden before the
  // navigation, so the total count of visible frames at unload time was 1.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       OutOfViewportInScrollableArea) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Move one of the fenced frames out of the viewport into scrollable area on
  // the page.
  FencedFrameVisibilityObserver out_of_viewport_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kRenderedOutOfViewport, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.style.position = 'absolute';
               ff_to_hide.style.top = (window.innerHeight + 10000) + 'px';
               ff_to_hide.style.left = (window.innerWidth + 10000) + 'px';
             )"));
  out_of_viewport_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. However, one of the frames was moved before the navigation,
  // so the total count of visible frames at unload time was 1.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       OutOfViewportScrollIntoView) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Move one of the fenced frames out of the viewport into scrollable area on
  // the page. In a dedicated block so the observer is destroyed when it goes
  // out of scope.
  {
    FencedFrameVisibilityObserver out_of_viewport_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        blink::mojom::FrameVisibility::kRenderedOutOfViewport, ff_to_hide);
    EXPECT_TRUE(ExecJs(primary_rfh,
                       R"(
              let fenced_frames = document.getElementsByTagName('fencedframe');
              let ff_to_hide = fenced_frames[fenced_frames.length - 1];
              ff_to_hide.style.position = 'absolute';
              ff_to_hide.style.top = (window.innerHeight + 10) + 'px';
            )"));
    out_of_viewport_observer.Wait();
  }

  // We now know that our moved frame is outside the viewport. Next, scroll the
  // moved frame back into view. Scrolling 30 pixels should put both of our
  // fenced frames partially into view.
  {
    FencedFrameVisibilityObserver scroll_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        blink::mojom::FrameVisibility::kRenderedInViewport, ff_to_hide);
    EXPECT_TRUE(ExecJs(primary_rfh,
                       R"(
        window.scroll({top: 30, left: 0, behavior: 'instant'});
      )"));
    scroll_observer.Wait();
  }

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. We moved one of the frames outside of the viewport, but
  // then scrolled it back into view. This means that at the time the page
  // unloads, there were 2 fenced frames rendered in the viewport.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       DetachFencedFrame) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_remove =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Detach one fenced frame from the DOM tree. This will cause it to leave the
  // viewport. Unlike other tests, trying to observe the visibility of the
  // detached frame's RFH is unsafe, because it may have been deleted.
  RenderFrameDeletedObserver remove_observer(ff_to_remove);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.remove();
             )"));
  remove_observer.WaitUntilDeleted();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. When we detached the fenced frame, it left the viewport, so
  // only one frame was present when the page unloaded.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest, ZeroOpacity) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Set one of the fenced frames to 0 opacity. There should be no visibility
  // change, meaning that it will stay rendered in the viewport, and our
  // observer should terminate immediately.
  FencedFrameVisibilityObserver opacity_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kRenderedInViewport, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let ff_to_hide = fenced_frames[fenced_frames.length - 1];
               ff_to_hide.style.opacity = 0;
             )"));
  opacity_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. An opacity change does not cause the frame to be rendered
  // out of the viewport, or not rendered at all. So the same-site count at
  // unload time is 2.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest, FullyObscured) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  RenderFrameHost* ff_to_hide =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Fully obscure the fenced frame. There should be no visibility change,
  // meaning that it will stay rendered in the viewport, even if it cannot
  // technically be seen, and our observer should terminate immediately.
  FencedFrameVisibilityObserver obscured_observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      blink::mojom::FrameVisibility::kRenderedInViewport, ff_to_hide);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
                let b = document.createElement("button");
                b.style.position = "absolute";
                b.style.top = 0;
                b.style.left = 0;
                b.style.width = "10000px";
                b.style.height = "10000px";
                b.style.zIndex = 1000;
                document.body.appendChild(b);
              )"));
  obscured_observer.Wait();

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 1. Even though the fenced frame was completely obscured, it
  // was still rendered in the viewport, so our same-site count at unload time
  // is still 1.
  ValidateHistograms(/*overall_max=*/1, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       SelfErrorNavigation) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* error_ff =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Force one of the fenced frames to navigate itself to an error page, by
  // first disabling untrusted network, and then attempting a navigation.
  EXPECT_TRUE(ExecJs(error_ff, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  const GURL fenced_frame_url_2(
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
  TestFrameNavigationObserver nav_observer(error_ff);
  EXPECT_TRUE(
      ExecJs(error_ff, JsReplace(R"(location.href = $1)", fenced_frame_url_2)));
  nav_observer.Wait();

  // We should have navigated to an error page with an opaque origin. We need to
  // get the most recent RFH for the fenced frame, because the error navigation
  // may have invalidated the old RFH.
  error_ff = ff_helper().GetMostRecentlyAddedFencedFrame(primary_rfh);
  EXPECT_TRUE(error_ff->IsErrorDocument());
  EXPECT_TRUE(error_ff->GetLastCommittedOrigin().opaque());

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. Error navigations will cause the error frame to be tracked
  // under its previous successfully committed site, which in this case was
  // a.test. So there are still 2 "a.test fenced frames" in the viewport at
  // unload time.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       SelfAboutBlankNavigation) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* blank_ff =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);
  url::Origin previous_origin = blank_ff->GetLastCommittedOrigin();

  // Force one of the fenced frames to navigate itself to about:blank.
  TestFrameNavigationObserver nav_observer(blank_ff);
  EXPECT_TRUE(ExecJs(blank_ff, R"(location.href = 'about:blank')"));
  nav_observer.Wait();

  // We should have navigated to about:blank. It inherits the origin of the
  // initiator, which in this case should be `previous_origin`. We need to get
  // the most recent RFH of the fenced frame, because it may have been
  // invalidated by the navigation.
  blank_ff = ff_helper().GetMostRecentlyAddedFencedFrame(primary_rfh);
  EXPECT_FALSE(blank_ff->IsErrorDocument());
  EXPECT_EQ(blank_ff->GetLastCommittedURL(), url::kAboutBlankURL);
  EXPECT_EQ(blank_ff->GetLastCommittedOrigin(), previous_origin);

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. Navigating to about:blank will cause the blank frame to
  // be tracked under its initiator's site, which in this case is still a.test.
  // There are still 2 "a.test fenced frames" in the viewport at unload time.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       EmbedderInitiatedAboutBlankNavigation) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* blank_ff =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Have the embedder navigate one of its fenced frames to about:blank.
  TestFrameNavigationObserver nav_observer(blank_ff);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
        let fenced_frames = document.getElementsByTagName('fencedframe');
        let blank_ff = fenced_frames[fenced_frames.length - 1];
        blank_ff.config = new FencedFrameConfig('about:blank');
      )"));
  nav_observer.Wait();

  // We should have navigated to about:blank. Fenced frame embedder-initiated
  // navigations have an opaque initiator origin to avoid leaking data into
  // the frame, see content::FencedFrame::Navigate() for more. We need to get
  // the most recent RFH of the fenced frame, because it may have been
  // invalidated by the navigation.
  blank_ff = ff_helper().GetMostRecentlyAddedFencedFrame(primary_rfh);
  EXPECT_FALSE(blank_ff->IsErrorDocument());
  EXPECT_EQ(blank_ff->GetLastCommittedURL(), url::kAboutBlankURL);
  EXPECT_TRUE(blank_ff->GetLastCommittedOrigin().opaque());

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. Navigating to about:blank will cause the blank frame to
  // be tracked under its opaque initiator. Therefore there is only 1 "a.test
  // fenced frame" in the viewport at unload time.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       RestoreFromBFCache) {
  // This test is only valid in BFCache configurations.
  if (!BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }

  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  RenderFrameHost* frame_to_remove =
      ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
          primary_rfh, fenced_frame_url);

  // Remove one of our fenced frames.
  RenderFrameDeletedObserver deleted_observer(frame_to_remove);
  EXPECT_TRUE(ExecJs(primary_rfh,
                     R"(
               let fenced_frames = document.getElementsByTagName('fencedframe');
               let frame_to_remove = fenced_frames[fenced_frames.length - 1];
               frame_to_remove.remove();
             )"));
  deleted_observer.WaitUntilDeleted();

  // Navigate the primary main frame, logging UMA metrics in the process. The
  // old primary RFH will enter BFCache.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));
  EXPECT_EQ(primary_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. At unload time, only 1 of our same-site frames still
  // existed. We're not calling ValidateHistograms here like the other tests,
  // because we expect to log multiple sets of metrics (keep reading :D ).
  int first_overall_max = 2;
  int first_unload_time_max = 1;

  RenderFrameHost* root = primary_main_frame_host();

  // Now, navigate the primary main frame back to restore from BFCache.
  TestFrameNavigationObserver back_observer(root);
  web_contents()->GetController().GoBack();
  back_observer.Wait();

  // The old primary_rfh has been restored from BFCache.
  EXPECT_EQ(primary_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kActive);

  // Create 2 more fenced frames.
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(primary_rfh,
                                                              fenced_frame_url);

  // Now, navigate the primary main frame forward. We'll put primary_rfh back
  // into BFCache again.
  TestFrameNavigationObserver forward_observer(root);
  web_contents()->GetController().GoForward();
  forward_observer.Wait();

  EXPECT_EQ(primary_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 1. Across the lifetime of the page, the highest number of same-site fenced
  //    frames in the viewport was 3. This is because one of the original 2
  //    frames was removed before entering BFCache the first time, so it was
  //    restored with only a single frame. Then, 2 more frames were added,
  //    bringing the count to 3.
  // 2. At unload time, all 3 of those frames were still in the viewport.
  int second_overall_max = 3;
  int second_unload_time_max = 3;

  // Now, finally, we get to check our histograms. We're not calling
  // ValidateHistograms because we need to examine multiple sets of metrics.
  // Each metric should be logged twice, in the first* and second* buckets we
  // created above.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  blink::kMaxSameSiteFencedFramesInViewportPerPageLoad),
              testing::ElementsAre(base::Bucket(first_overall_max, 1),
                                   base::Bucket(second_overall_max, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  blink::kMaxSameSiteFencedFramesInViewportAtUnload),
              testing::ElementsAre(base::Bucket(first_unload_time_max, 1),
                                   base::Bucket(second_unload_time_max, 1)));
}

IN_PROC_BROWSER_TEST_F(FencedFrameViewportObserverBrowserTest,
                       CrossOriginSameSite) {
  const GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  const GURL fenced_frame_url_foo(
      https_server()->GetURL("foo.a.test", "/fenced_frames/title1.html"));
  const GURL fenced_frame_url_bar(
      https_server()->GetURL("bar.a.test", "/fenced_frames/title1.html"));

  // These two URLs are same-site (a.test), but cross-origin because they have
  // different hostnames.
  EXPECT_FALSE(
      url::Origin::Create(fenced_frame_url_foo)
          .IsSameOriginWith(url::Origin::Create(fenced_frame_url_bar)));

  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
      primary_rfh, fenced_frame_url_foo);
  ff_helper().CreateFencedFrameAndWaitUntilRenderedInViewport(
      primary_rfh, fenced_frame_url_bar);

  // Navigate the primary main frame, logging UMA metrics in the process.
  const GURL new_main_url(https_server()->GetURL("b.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), new_main_url));

  // Across the lifetime of the page, the most same-site fenced frames in the
  // viewport was 2. When the primary main frame navigated away, our 2
  // same-site fenced frames were still in the viewport. Even though our 2
  // frames were cross-origin to one another, they were still same-site, so both
  // counted towards the metrics.
  ValidateHistograms(/*overall_max=*/2, /*unload_time_max=*/2);
}

}  // namespace

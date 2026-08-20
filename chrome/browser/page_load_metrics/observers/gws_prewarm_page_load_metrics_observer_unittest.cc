// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_prewarm_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGoogleSearchPrewarmUrl[] =
    "https://www.google.com/search/warmup.html";
constexpr char kGoogleSearchUrl[] = "https://www.google.com/search?q=test";
constexpr char kNonGoogleUrl[] = "https://www.example.com/";

class GWSPrewarmPageLoadMetricsObserverTest : public testing::Test {
 public:
  GWSPrewarmPageLoadMetricsObserverTest() = default;
};

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonPrerenderNavigationIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnStart(&handle, GURL(), /*started_in_foreground=*/true));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, FencedFramesIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnFencedFramesStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonPrewarmPrerenderIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonGooglePrerenderIgnored) {
  content::MockNavigationHandle handle(GURL(kNonGoogleUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, PrewarmPrerenderObservedCached) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  handle.set_was_response_cached(true);

  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&handle));
  EXPECT_TRUE(observer.was_cached());
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasResponseCached, true, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       PrewarmPrerenderObservedNotCached) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  handle.set_was_response_cached(false);

  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&handle));
  EXPECT_FALSE(observer.was_cached());
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasResponseCached, false, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       CommitRedirectToNonPrewarmUrlStopsObserving) {
  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  content::MockNavigationHandle commit_handle(GURL(kGoogleSearchUrl),
                                              /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnCommit(&commit_handle));
}

}  // namespace

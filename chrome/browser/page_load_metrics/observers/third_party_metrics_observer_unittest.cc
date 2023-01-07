// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead2";
const char kWriteCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieWrite2";
const char kAccessLocalStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess2";
const char kAccessSessionStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess2";
const char kSubframeFCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3";

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

class ThirdPartyMetricsObserverTestBase
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  ThirdPartyMetricsObserverTestBase(const ThirdPartyMetricsObserverTestBase&) =
      delete;
  ThirdPartyMetricsObserverTestBase& operator=(
      const ThirdPartyMetricsObserverTestBase&) = delete;

 protected:
  ThirdPartyMetricsObserverTestBase() = default;

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new ThirdPartyMetricsObserver()));
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 content::RenderFrameHost* frame) {
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  content::RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetPrimaryMainFrame());
  }

  RenderFrameHost* AppendChildFrame(content::RenderFrameHost* parent) {
    if (WithFencedFrames())
      return content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return content::RenderFrameHostTester::For(parent)->AppendChild("iframe");
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             content::RenderFrameHost* parent) {
    return NavigateFrame(url, AppendChildFrame(parent));
  }

  virtual bool WithFencedFrames() = 0;
};

class ThirdPartyMetricsObserverTest : public ThirdPartyMetricsObserverTestBase,
                                      public testing::WithParamInterface<bool> {
 private:
  bool WithFencedFrames() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, ThirdPartyMetricsObserverTest, testing::Bool());

TEST_P(ThirdPartyMetricsObserverTest, NoThirdPartyFrame_NoneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://a.top.com/foo", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 0);
}

TEST_P(ThirdPartyMetricsObserverTest, OneThirdPartyFrame_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       OneThirdPartyFrameWithTwoSameUpdates_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_P(ThirdPartyMetricsObserverTest, SixtyFrames_FiftyRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");

  // Add more frames than we're supposed to track.
  for (int i = 0; i < 60; ++i) {
    RenderFrameHost* sub_frame =
        CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.paint_timing->first_contentful_paint = base::Seconds(1);
    tester()->SimulateTimingUpdate(timing, sub_frame);
  }

  // Keep this synchronized w/ the max frame count in the cc file.
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 50);
}

TEST_P(ThirdPartyMetricsObserverTest, ThreeThirdPartyFrames_ThreeRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");

  // Create three third-party frames.
  RenderFrameHost* sub_frame_a =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);
  RenderFrameHost* sub_frame_b =
      CreateAndNavigateSubFrame("https://y-origin.com", main_frame);
  RenderFrameHost* sub_frame_c =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  // Create a same-origin frame.
  RenderFrameHost* sub_frame_d =
      CreateAndNavigateSubFrame("https://top.com/foo", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame_a);

  timing.paint_timing->first_contentful_paint = base::Seconds(2);
  tester()->SimulateTimingUpdate(timing, sub_frame_b);

  timing.paint_timing->first_contentful_paint = base::Seconds(3);
  tester()->SimulateTimingUpdate(timing, sub_frame_c);

  timing.paint_timing->first_contentful_paint = base::Seconds(4);
  tester()->SimulateTimingUpdate(timing, sub_frame_d);

  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 3);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(1), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(2), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(kSubframeFCPHistogram,
                                                     base::Seconds(3), 1);
}

TEST_P(ThirdPartyMetricsObserverTest, NoCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, BlockedCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy reads, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party reads.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  true /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(kReadCookieHistogram, 0);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       DifferentSchemeSameRegistrableDomain_OneRecorded) {
  NavigateAndCommit(GURL("http://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://top.com"),
                                  GURL("http://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OnlyFirstPartyCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://top.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OneCookieRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       ThreeCookiesReadSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com/foo"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://sub.a.com/bar"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       CookiesReadMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 2, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, NoCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, BlockedCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy writes, nothing should be recorded. Even
  // if there are non-blocked third-party writes.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  true /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(kWriteCookieHistogram, 0);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  url,
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       OnlyFirstPartyCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://top.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, OneCookieChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       TwoCookiesChangeSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       CookiesChangeMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 2, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, ReadAndChangeCookies_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kRead,
                                  GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->SimulateCookieAccess({content::CookieAccessDetails::Type::kChange,
                                  GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  {net::CanonicalCookie()},
                                  false /* blocked_by_policy */});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest, NoDomStorageAccess_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram,
                                                  0, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      kAccessSessionStorageHistogram, 0, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       LocalAndSessionStorageAccess_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(
      GURL("https://a.com"), GURL("https://top.com"),
      false /* blocked_by_policy */,
      page_load_metrics::StorageType::kLocalStorage /* storage_type */);
  tester()->SimulateStorageAccess(
      GURL("https://a.com"), GURL("https://top.com"),
      false /* blocked_by_policy */,
      page_load_metrics::StorageType::kSessionStorage /* storage_type */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram,
                                                  1, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      kAccessSessionStorageHistogram, 1, 1);
}

TEST_P(ThirdPartyMetricsObserverTest,
       LargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;

  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://foo.test"));
  tester()->SimulateTimingUpdate(timing);

  int frame_tree_node_id = main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("https://bar.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.Clients.ThirdParty.PaintTiming."
                  "NavigationToLargestContentfulPaint.HasThirdPartyFont"),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoLargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;

  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("http://a.foo.test"));
  tester()->SimulateTimingUpdate(timing);

  // Load a same-site font, the histogram should not be recorded.
  int frame_tree_node_id = main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("http://b.foo.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.ThirdParty.PaintTiming."
      "NavigationToLargestContentfulPaint.HasThirdPartyFont",
      0);
}

TEST_P(ThirdPartyMetricsObserverTest,
       NoTextLargestContentfulPaint_HasThirdPartyFont) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      120u;

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://foo.test"));
  tester()->SimulateTimingUpdate(timing);

  int frame_tree_node_id = main_rfh()->GetFrameTreeNodeId();
  tester()->SimulateLoadedResource(
      {url::SchemeHostPort(GURL("https://bar.test")), net::IPEndPoint(),
       frame_tree_node_id, false /* was_cached */,
       1024 * 20 /* raw_body_bytes */, 0 /* original_network_content_length */,
       network::mojom::RequestDestination::kFont, 0,
       nullptr /* load_timing_info */},
      content::GlobalRequestID());

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL("https://foo.test"));

  // Since largest contentful paint is of type image, the histogram will not be
  // recorded.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.ThirdParty.PaintTiming."
      "NavigationToLargestContentfulPaint.HasThirdPartyFont",
      0);
}

enum class TestType {
  LocalWithIframes,
  LocalWithFencedFrames,
  NotLocalWithIframes,
  NotLocalWithFencedFrames,
};

class ThirdPartyDomStorageAccessMetricsObserverTest
    : public ThirdPartyMetricsObserverTestBase,
      public ::testing::WithParamInterface<enum TestType> {
 public:
  bool IsLocal() const {
    auto test_type = GetParam();
    return test_type == TestType::LocalWithIframes ||
           test_type == TestType::LocalWithFencedFrames;
  }

  page_load_metrics::StorageType StorageType() const {
    return IsLocal() ? page_load_metrics::StorageType::kLocalStorage
                     : page_load_metrics::StorageType::kSessionStorage;
  }

  const char* DomStorageHistogramName() const {
    return IsLocal() ? kAccessLocalStorageHistogram
                     : kAccessSessionStorageHistogram;
  }

 private:
  // Implements ThirdPartyMetricsObserverTestBase.
  bool WithFencedFrames() override {
    auto test_type = GetParam();
    return test_type == TestType::LocalWithFencedFrames ||
           test_type == TestType::NotLocalWithFencedFrames;
  }
};

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest, Blocked_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy access, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party access.
  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  true /* blocked_by_policy */, StorageType());
  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(DomStorageHistogramName(), 0);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       NoRegistrableDomainNoHost_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(GURL("data:,Hello%2C%20World!"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 0,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       NoRegistrableDomainWithHost_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(GURL("https://127.0.0.1"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest, SameOrigin_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(GURL("https://top.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 0,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       DifferentOrigin_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       DifferentSchemeSameRegistrableDomain_OneRecorded) {
  NavigateAndCommit(GURL("http://top.com"));

  tester()->SimulateStorageAccess(GURL("https://top.com"),
                                  GURL("http://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(
    ThirdPartyDomStorageAccessMetricsObserverTest,
    TwoAccesses_BothSameSchemeAndRegistrableDomainDifferentOrigin_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->SimulateStorageAccess(GURL("https://sub.a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       ThreeAccesses_TwoOrigins_TwoRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party DOM storage access from two different
  // origins.
  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->SimulateStorageAccess(GURL("https://a.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->SimulateStorageAccess(GURL("https://b.com"),
                                  GURL("https://top.com"),
                                  false /* blocked_by_policy */, StorageType());
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 2,
                                                  1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThirdPartyDomStorageAccessMetricsObserverTest,
                         ::testing::Values(TestType::LocalWithIframes,
                                           TestType::LocalWithFencedFrames,
                                           TestType::NotLocalWithIframes,
                                           TestType::NotLocalWithFencedFrames));

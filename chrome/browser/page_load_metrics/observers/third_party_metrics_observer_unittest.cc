// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_metrics_observer.h"

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead";
const char kWriteCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieWrite";
const char kAccessLocalStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.LocalStorageAccess";
const char kAccessSessionStorageHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.SessionStorageAccess";
const char kSubframeFCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint";

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;

class ThirdPartyMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  ThirdPartyMetricsObserverTest() {}

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
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             content::RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild("frame_name");
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), subframe);
    navigation_simulator->Commit();

    return navigation_simulator->GetFinalRenderFrameHost();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsObserverTest);
};

TEST_F(ThirdPartyMetricsObserverTest, NoThirdPartyFrame_NoneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://top.com/foo", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 0);
}

TEST_F(ThirdPartyMetricsObserverTest, OneThirdPartyFrame_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       OneThirdPartyFrameWithTwoSameUpdates_OneRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->SimulateTimingUpdate(timing, sub_frame);
  tester()->histogram_tester().ExpectUniqueSample(kSubframeFCPHistogram, 1000,
                                                  1);
}

TEST_F(ThirdPartyMetricsObserverTest, SixtyFrames_FiftyRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame("https://top.com");

  // Add more frames than we're supposed to track.
  for (int i = 0; i < 60; ++i) {
    RenderFrameHost* sub_frame =
        CreateAndNavigateSubFrame("https://x-origin.com", main_frame);

    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(1);
    tester()->SimulateTimingUpdate(timing, sub_frame);
  }

  // Keep this synchronized w/ the max frame count in the cc file.
  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 50);
}

TEST_F(ThirdPartyMetricsObserverTest, ThreeThirdPartyFrames_ThreeRecorded) {
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
  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(1);
  tester()->SimulateTimingUpdate(timing, sub_frame_a);

  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(2);
  tester()->SimulateTimingUpdate(timing, sub_frame_b);

  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(3);
  tester()->SimulateTimingUpdate(timing, sub_frame_c);

  timing.paint_timing->first_contentful_paint = base::TimeDelta::FromSeconds(4);
  tester()->SimulateTimingUpdate(timing, sub_frame_d);

  tester()->histogram_tester().ExpectTotalCount(kSubframeFCPHistogram, 3);
  tester()->histogram_tester().ExpectTimeBucketCount(
      kSubframeFCPHistogram, base::TimeDelta::FromSeconds(1), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(
      kSubframeFCPHistogram, base::TimeDelta::FromSeconds(2), 1);
  tester()->histogram_tester().ExpectTimeBucketCount(
      kSubframeFCPHistogram, base::TimeDelta::FromSeconds(3), 1);
}

TEST_F(ThirdPartyMetricsObserverTest, NoCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, BlockedCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy reads, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party reads.
  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                true /* blocked_by_policy */);
  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(kReadCookieHistogram, 0);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesRead_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  tester()->SimulateCookiesRead(url, GURL("https://top.com"), net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  tester()->SimulateCookiesRead(url, GURL("https://top.com"), net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OnlyFirstPartyCookiesRead_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookiesRead(GURL("https://top.com"),
                                GURL("https://top.com"), net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OneCookieRead_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       ThreeCookiesReadSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->SimulateCookiesRead(GURL("https://a.com/foo"),
                                GURL("https://top.com"), net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->SimulateCookiesRead(GURL("https://sub.a.com/bar"),
                                GURL("https://top.com"), net::CookieList(),
                                false /* blocked_by_policy */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       CookiesReadMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->SimulateCookiesRead(GURL("https://b.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 2, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, NoCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, BlockedCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy writes, nothing should be recorded. Even
  // if there are non-blocked third-party writes.
  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 true /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(kWriteCookieHistogram, 0);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainNoHostCookiesChanged_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("data:,Hello%2C%20World!");
  ASSERT_FALSE(url.has_host());
  tester()->SimulateCookieChange(url, GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       NoRegistrableDomainWithHostCookiesChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  GURL url = GURL("https://127.0.0.1/cookies");
  ASSERT_TRUE(url.has_host());
  tester()->SimulateCookieChange(url, GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       OnlyFirstPartyCookiesChanged_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieChange(
      GURL("https://top.com"), GURL("https://top.com"), net::CanonicalCookie(),
      false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, OneCookieChanged_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       TwoCookiesChangeSameThirdParty_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       CookiesChangeMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->SimulateCookieChange(GURL("https://a.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->SimulateCookieChange(GURL("https://b.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 2, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, ReadAndChangeCookies_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party cookie reads from two different origins.
  tester()->SimulateCookiesRead(GURL("https://a.com"), GURL("https://top.com"),
                                net::CookieList(),
                                false /* blocked_by_policy */);
  tester()->SimulateCookieChange(GURL("https://b.com"), GURL("https://top.com"),
                                 net::CanonicalCookie(),
                                 false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  tester()->histogram_tester().ExpectUniqueSample(kWriteCookieHistogram, 1, 1);
}

TEST_F(ThirdPartyMetricsObserverTest, NoDomStorageAccess_NoneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram,
                                                  0, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      kAccessSessionStorageHistogram, 0, 1);
}

TEST_F(ThirdPartyMetricsObserverTest,
       LocalAndSessionStorageAccess_BothRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), true /* local */,
                                     false /* blocked_by_policy */);
  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), false /* local */,
                                     false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(kAccessLocalStorageHistogram,
                                                  1, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      kAccessSessionStorageHistogram, 1, 1);
}

class ThirdPartyDomStorageAccessMetricsObserverTest
    : public ThirdPartyMetricsObserverTest,
      public ::testing::WithParamInterface<bool /* is_local_access */> {
 public:
  bool IsLocal() const { return GetParam(); }

  const char* DomStorageHistogramName() const {
    return IsLocal() ? kAccessLocalStorageHistogram
                     : kAccessSessionStorageHistogram;
  }
};

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       BlockedDomStorageAccess_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // If there are any blocked_by_policy access, nothing should be recorded. Even
  // if there are subsequent non-blocked third-party access.
  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     true /* blocked_by_policy */);
  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(DomStorageHistogramName(), 0);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       NoRegistrableDomainDomStorageAccess_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateDomStorageAccess(GURL("data:,Hello%2C%20World!"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       OnlyFirstPartyDomStorageAccess_NotRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateDomStorageAccess(GURL("https://top.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 0,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       OneDomStorageAccess_OneRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 1,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       SameRegistrableDomainDifferentOrigin_TwoRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->SimulateDomStorageAccess(GURL("https://sub.a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 2,
                                                  1);
}

TEST_P(ThirdPartyDomStorageAccessMetricsObserverTest,
       DomStorageAccessMultipleThirdParties_MultipleRecorded) {
  NavigateAndCommit(GURL("https://top.com"));

  // Simulate third-party DOM storage access from two different
  // origins.
  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->SimulateDomStorageAccess(GURL("https://a.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->SimulateDomStorageAccess(GURL("https://b.com"),
                                     GURL("https://top.com"), IsLocal(),
                                     false /* blocked_by_policy */);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(DomStorageHistogramName(), 2,
                                                  1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ThirdPartyDomStorageAccessMetricsObserverTest,
    ::testing::Values(false, true));

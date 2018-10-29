// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_tester.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/host_port_pair.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

using content::TestNavigationThrottle;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::NavigationSimulator;

namespace {

struct ExpectedFrameBytes {
  ExpectedFrameBytes(size_t cached_kb, size_t uncached_kb)
      : cached_kb(cached_kb), uncached_kb(uncached_kb) {}
  size_t cached_kb;
  size_t uncached_kb;
};

enum class AdType { GOOGLE = 0, SUBRESOURCE_FILTER = 1, ALL = 2 };
enum class ResourceCached { NOT_CACHED, CACHED };
enum class FrameType { AD = 0, NON_AD };

const char kAdUrl[] = "https://tpc.googlesyndication.com/safeframe/1";
const char kNonAdUrl[] = "https://foo.com/";
const char kNonAdUrl2[] = "https://bar.com/";
const char kNonAdUrlSameOrigin[] = "https://tpc.googlesyndication.com/nonad";

const char kAdName[] = "google_ads_iframe_1";
const char kNonAdName[] = "foo";

// Asynchronously cancels the navigation at WillProcessResponse. Before
// cancelling, simulates loading a main frame resource.
class ResourceLoadingCancellingThrottle
    : public content::TestNavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> Create(
      content::NavigationHandle* handle) {
    return std::make_unique<ResourceLoadingCancellingThrottle>(handle);
  }

  explicit ResourceLoadingCancellingThrottle(
      content::NavigationHandle* navigation_handle)
      : content::TestNavigationThrottle(navigation_handle) {
    SetResponse(TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                TestNavigationThrottle::ASYNCHRONOUS, CANCEL);
  }

 private:
  // content::TestNavigationThrottle:
  void OnWillRespond(NavigationThrottle::ThrottleCheckResult result) {
    if (result.action() != CANCEL) {
      return;
    }

    auto* observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            navigation_handle()->GetWebContents());
    DCHECK(observer);

    // Load a resource for the main frame before it commits.
    observer->OnRequestComplete(
        GURL(kNonAdUrl), net::HostPortPair(),
        navigation_handle()->GetRenderFrameHost()->GetFrameTreeNodeId(),
        navigation_handle()->GetGlobalRequestID(),
        navigation_handle()->GetRenderFrameHost(),
        content::RESOURCE_TYPE_MAIN_FRAME, false /* was_cached */,
        nullptr /* data_reduction_proxy */, 10 * 1024 /* raw_body_bytes */,
        0 /* original_network_content_length */, base::TimeTicks::Now(), 0,
        nullptr /* load_timing_info */);
  }

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingCancellingThrottle);
};

std::string AdTypeToString(AdType ad_type) {
  switch (ad_type) {
    case AdType::GOOGLE:
      return "Google";
    case AdType::SUBRESOURCE_FILTER:
      return "SubresourceFilter";
    case AdType::ALL:
      return "All";
  }
  ADD_FAILURE();
  return "";
}

std::string TypedHistogram(const std::string& suffix, AdType ad_type) {
  return base::StringPrintf("PageLoad.Clients.Ads.%s.%s",
                            AdTypeToString(ad_type).c_str(), suffix.c_str());
}

// Verifies that the histograms match what is expected given |google_ad_frames|
// ad frame byte counts and non-ad counts (|other_cached_kb| and
// |other_uncached_kb|).
void TestHistograms(const base::HistogramTester& histograms,
                    const std::vector<ExpectedFrameBytes>& google_ad_frames,
                    size_t non_ad_cached_kb,
                    size_t non_ad_uncached_kb,
                    AdType ad_type) {
  size_t total_ad_cached_kb = 0;
  size_t total_ad_uncached_kb = 0;
  size_t total_ad_kb = 0;
  size_t ad_frame_count = 0;

  std::map<size_t, int> frames_with_total_byte_count;
  std::map<size_t, int> frames_with_network_byte_count;
  std::map<size_t, int> frames_with_percent_network_count;

  // Perform some initial calculations on the number of bytes, of each type,
  // in each ad frame.
  for (const ExpectedFrameBytes& bytes : google_ad_frames) {
    total_ad_cached_kb += bytes.cached_kb;
    total_ad_uncached_kb += bytes.uncached_kb;
    total_ad_kb += bytes.cached_kb + bytes.uncached_kb;

    if (total_ad_kb == 0)
      continue;

    ad_frame_count += 1;

    size_t total_frame_kb = bytes.cached_kb + bytes.uncached_kb;

    frames_with_total_byte_count[total_frame_kb] += 1;
    frames_with_network_byte_count[bytes.uncached_kb] += 1;
    frames_with_percent_network_count[(bytes.uncached_kb * 100) /
                                      total_frame_kb] += 1;
  }

  // Test the histograms.
  histograms.ExpectUniqueSample(
      TypedHistogram("FrameCounts.AnyParentFrame.AdFrames", ad_type),
      ad_frame_count, 1);

  if (ad_frame_count == 0)
    return;

  for (const auto& total_bytes_and_count : frames_with_total_byte_count) {
    histograms.ExpectBucketCount(
        TypedHistogram("Bytes.AdFrames.PerFrame.Total", ad_type),
        total_bytes_and_count.first, total_bytes_and_count.second);
  }
  for (const auto& network_bytes_and_count : frames_with_network_byte_count) {
    histograms.ExpectBucketCount(
        TypedHistogram("Bytes.AdFrames.PerFrame.Network", ad_type),
        network_bytes_and_count.first, network_bytes_and_count.second);
  }
  for (const auto& percent_network_and_count :
       frames_with_percent_network_count) {
    histograms.ExpectBucketCount(
        TypedHistogram("Bytes.AdFrames.PerFrame.PercentNetwork", ad_type),
        percent_network_and_count.first, percent_network_and_count.second);
  }

  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.AdFrames.Aggregate.Total", ad_type), total_ad_kb,
      1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.AdFrames.Aggregate.Network", ad_type),
      total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.FullPage.Total", ad_type),
      non_ad_cached_kb + non_ad_uncached_kb + total_ad_kb, 1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.FullPage.Network", ad_type),
      non_ad_uncached_kb + total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.NonAdFrames.Aggregate.Total", ad_type),
      non_ad_cached_kb + non_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.FullPage.Total.PercentAds", ad_type),
      (total_ad_kb * 100) /
          (total_ad_kb + non_ad_cached_kb + non_ad_uncached_kb),
      1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.AdFrames.Aggregate.PercentNetwork", ad_type),
      ((total_ad_uncached_kb * 100) / total_ad_kb), 1);
  histograms.ExpectUniqueSample(
      TypedHistogram("Bytes.FullPage.Network.PercentAds", ad_type),
      (total_ad_uncached_kb * 100) /
          (total_ad_uncached_kb + non_ad_uncached_kb),
      1);
}

}  // namespace

class AdsPageLoadMetricsObserverTest : public SubresourceFilterTestHarness {
 public:
  AdsPageLoadMetricsObserverTest() {}

  void SetUp() override {
    SubresourceFilterTestHarness::SetUp();
    tester_ =
        std::make_unique<page_load_metrics::PageLoadMetricsObserverTester>(
            web_contents(),
            base::BindRepeating(
                &AdsPageLoadMetricsObserverTest::RegisterObservers,
                base::Unretained(this)));
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
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             const std::string& frame_name,
                                             content::RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), subframe);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  void LoadResource(RenderFrameHost* frame,
                    ResourceCached resource_cached,
                    int resource_size_in_kb) {
    page_load_metrics::ExtraRequestCompleteInfo request(
        GURL(kNonAdUrl), net::HostPortPair(), frame->GetFrameTreeNodeId(),
        resource_cached == ResourceCached::CACHED, resource_size_in_kb * 1024,
        0,       /* original_network_content_length */
        nullptr, /* data_reduction_proxy_data */
        content::RESOURCE_TYPE_SUB_FRAME, 0, nullptr /* load_timing_info */);
    tester_->SimulateLoadedResource(request);
  }

  void ResourceDataUpdate(int resource_size_in_kbyte,
                          std::string mime_type,
                          bool is_ad_resource) {
    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    page_load_metrics::mojom::ResourceDataUpdatePtr resource =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource->received_data_length = resource_size_in_kbyte;
    resource->delta_bytes = resource_size_in_kbyte;
    resource->reported_as_ad_resource = is_ad_resource;
    resource->is_complete = true;
    resource->mime_type = mime_type;
    resources.push_back(std::move(resource));
    tester_->SimulateResourceDataUseUpdate(resources);
  }

  void TimingUpdate(const page_load_metrics::mojom::PageLoadTiming& timing) {
    tester_->SimulateTimingUpdate(timing);
  }

  page_load_metrics::PageLoadMetricsObserverTester* tester() {
    return tester_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    tracker->AddObserver(std::make_unique<AdsPageLoadMetricsObserver>());
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester> tester_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverTest);
};

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNoAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  RenderFrameHost* frame2 =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(frame1, ResourceCached::NOT_CACHED, 10);
  LoadResource(frame2, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), std::vector<ExpectedFrameBytes>(),
                 0 /* non_ad_cached_kb */, 30 /* non_ad_uncached_kb */,
                 AdType::GOOGLE);

  // Verify that other UMA wasn't written.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, ResourceBeforeAdFrameCommits) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Assume that the next frame's id will be the main frame + 1 and load a
  // resource for that frame. Make sure it gets counted.
  page_load_metrics::ExtraRequestCompleteInfo request(
      GURL(kNonAdUrl), net::HostPortPair(),
      main_frame->GetFrameTreeNodeId() + 1, false /* cached */,
      10 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_SUB_FRAME, 0, nullptr /* load_timing_info */);
  tester()->SimulateLoadedResource(request);

  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 10}}, 0 /* non_ad_cached_kb */,
                 10 /*non_ad_uncached_kb*/, AdType::GOOGLE);
}

TEST_F(AdsPageLoadMetricsObserverTest, AllAdTypesInPage) {
  // Make this page DRYRUN.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kDryRun,
      subresource_filter::ActivationScope::ALL_SITES));

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  RenderFrameHost* non_ad_frame2 =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Create 5 ad frames with the 5th nested inside the 4th. Verify that the
  // nested ad frame doesn't get counted separately (but that its bytes are
  // still counted). Also verify that the various ad signals (urls and names)
  // are properly detected.
  RenderFrameHost* google_frame1 =
      CreateAndNavigateSubFrame(kNonAdUrl, "google_ads_iframe_1", main_frame);
  RenderFrameHost* google_frame2 =
      CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);

  RenderFrameHost* srf_frame1 =
      CreateAndNavigateSubFrame(kDefaultDisallowedUrl, kNonAdName, main_frame);
  RenderFrameHost* srf_frame2 =
      CreateAndNavigateSubFrame(kDefaultDisallowedUrl, kNonAdName, main_frame);
  RenderFrameHost* nested_srf_frame3 =
      CreateAndNavigateSubFrame(kDefaultDisallowedUrl, kNonAdName, srf_frame2);

  // Create an addditional ad frame without content. It shouldn't be counted
  // as an ad frame.
  CreateAndNavigateSubFrame(kDefaultDisallowedUrl, kNonAdName, main_frame);

  // 70KB total in page, 50 from ads, 40 from network, and 30 of those
  // are from ads.
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(non_ad_frame, ResourceCached::CACHED, 10);
  LoadResource(non_ad_frame2, ResourceCached::CACHED, 10);
  LoadResource(google_frame1, ResourceCached::CACHED, 10);
  LoadResource(google_frame2, ResourceCached::NOT_CACHED, 10);
  LoadResource(srf_frame1, ResourceCached::NOT_CACHED, 10);
  LoadResource(srf_frame2, ResourceCached::NOT_CACHED, 10);
  LoadResource(nested_srf_frame3, ResourceCached::CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{10, 0}, {0, 10}},
                 30 /* non_ad_cached_kb */, 30 /* non_ad_uncached_kb */,
                 AdType::GOOGLE);
  TestHistograms(histogram_tester(), {{0, 10}, {10, 10}},
                 30 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */,
                 AdType::SUBRESOURCE_FILTER);
  TestHistograms(histogram_tester(), {{10, 0}, {0, 10}, {0, 10}, {10, 10}},
                 20 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */,
                 AdType::ALL);
}

// Test that the cross-origin ad subframe navigation metric works as it's
// supposed to, triggering a false addition with each ad that's in the same
// origin as the main page, and a true when when the ad has a separate origin.
TEST_F(AdsPageLoadMetricsObserverTest, AdsOriginStatusMetrics) {
  const char kCrossOriginHistogramId[] =
      "PageLoad.Clients.Ads.Google.FrameCounts.AdFrames.PerFrame.OriginStatus";

  // Test that when the main frame origin is different from a direct ad
  // subframe it is correctly identified as cross-origin, but do not count
  // indirect ad subframes.
  {
    base::HistogramTester histograms;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    RenderFrameHost* ad_sub_frame =
        CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);
    LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
    LoadResource(ad_sub_frame, ResourceCached::NOT_CACHED, 10);
    LoadResource(CreateAndNavigateSubFrame(kAdUrl, kNonAdName, ad_sub_frame),
                 ResourceCached::NOT_CACHED, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(
        kCrossOriginHistogramId,
        AdsPageLoadMetricsObserver::AdOriginStatus::kCross, 1);
  }

  // Add a non-ad subframe and an ad subframe and make sure the total count
  // only adjusts by one.
  {
    base::HistogramTester histograms;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
    LoadResource(CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame),
                 ResourceCached::NOT_CACHED, 10);
    LoadResource(CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame),
                 ResourceCached::NOT_CACHED, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(
        kCrossOriginHistogramId,
        AdsPageLoadMetricsObserver::AdOriginStatus::kCross, 1);
  }

  // Add an ad subframe in the same origin as the parent frame and make sure it
  // gets identified as non-cross-origin. Note: top-level navigations are never
  // considered to be ads.
  {
    base::HistogramTester histograms;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrlSameOrigin);
    LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
    LoadResource(CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame),
                 ResourceCached::NOT_CACHED, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(
        kCrossOriginHistogramId,
        AdsPageLoadMetricsObserver::AdOriginStatus::kSame, 1);
  }
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithAdFrameThatRenavigates) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(ad_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate the ad frame again.
  ad_frame = NavigateFrame(kNonAdUrl, ad_frame);

  // In total, 30KB for entire page and 20 in one ad frame.
  LoadResource(ad_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 20}}, 0 /* non_ad_cached_kb */,
                 10 /* non_ad_uncached_kb */, AdType::GOOGLE);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNonAdFrameThatRenavigatesToAd) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Child of the sub-frame that is an ad.
  RenderFrameHost* sub_frame_child_ad =
      CreateAndNavigateSubFrame(kNonAdUrl2, kAdName, sub_frame);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame_child_ad, ResourceCached::NOT_CACHED, 10);

  // Navigate the subframe again, this time it's an ad.
  sub_frame = NavigateFrame(kAdUrl, sub_frame);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // In total, 40KB was loaded for the entire page and 20KB from ad
  // frames (the original child ad frame and the renavigated frame which
  // turned into an ad).

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 10}, {0, 10}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */,
                 AdType::GOOGLE);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedNavigation) {
  // If the first navigation in a frame is aborted, keep track of its bytes.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Create an ad subframe that aborts before committing.
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild(kAdName);
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), subframe_ad);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). They should
  // be counted.
  LoadResource(subframe_ad, ResourceCached::NOT_CACHED, 10);
  LoadResource(subframe_ad, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 20}}, 0 /* non_ad_cached_kb */,
                 10 /* non_ad_uncached_kb */, AdType::GOOGLE);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedSecondNavigationForFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // Now navigate (and abort) the subframe to an ad.
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), sub_frame);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). Since the
  // frame attempted to load an ad, the frame is tagged forever as an ad.
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 20}}, 0 /* non_ad_cached_kb */,
                 20 /* non_ad_uncached_kb */, AdType::GOOGLE);
}

TEST_F(AdsPageLoadMetricsObserverTest, TwoResourceLoadsBeforeCommit) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Now open a subframe and have its resource load before notification of
  // navigation finishing.
  page_load_metrics::ExtraRequestCompleteInfo request(
      GURL(kNonAdUrl), net::HostPortPair(),
      main_frame->GetFrameTreeNodeId() + 1, false /* cached */,
      10 * 1024 /* size */, false /* data_reduction_proxy_used */,
      0 /* original_network_content_length */, content::RESOURCE_TYPE_SUB_FRAME,
      0, nullptr /* load_timing_info */);
  tester()->SimulateLoadedResource(request);
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild(kAdName);
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), subframe_ad);

  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Renavigate the subframe to a successful commit. But again, the resource
  // loads before the observer sees the finished navigation.
  tester()->SimulateLoadedResource(request);
  NavigateFrame(kNonAdUrl, subframe_ad);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), {{0, 20}}, 0 /* non_ad_cached_kb */,
                 10 /* non_ad_uncached_kb */, AdType::GOOGLE);
}

// This tests an issue that is believed to be the cause of
// https://crbug.com/721369. The issue is that a frame from a previous
// navigation might commit during a new navigation, and the ads metrics won't
// know about the frame's parent (because it doesn't exist in the page).
TEST_F(AdsPageLoadMetricsObserverTest, FrameWithNoParent) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Renavigate the child, but, while navigating, the main frame renavigates.
  RenderFrameHost* child_of_subframe =
      RenderFrameHostTester::For(sub_frame)->AppendChild(kAdName);
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl2), child_of_subframe);
  navigation_simulator->Start();

  // Main frame renavigates.
  NavigateMainFrame(kNonAdUrl);

  // Child frame commits.
  navigation_simulator->Commit();
  child_of_subframe = navigation_simulator->GetFinalRenderFrameHost();

  // Test that a resource loaded into an unknown frame doesn't cause any
  // issues.
  LoadResource(child_of_subframe, ResourceCached::NOT_CACHED, 10);
}

TEST_F(AdsPageLoadMetricsObserverTest, MainFrameResource) {
  // Start main-frame navigation
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), web_contents()->GetMainFrame());
  navigation_simulator->Start();
  int frame_tree_node_id =
      navigation_simulator->GetNavigationHandle()->GetFrameTreeNodeId();
  navigation_simulator->Commit();

  page_load_metrics::ExtraRequestCompleteInfo request(
      GURL(kNonAdUrl), net::HostPortPair(), frame_tree_node_id,
      false /* was_cached */, 10 * 1024 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_MAIN_FRAME, 0, nullptr /* load_timing_info */);

  tester()->SimulateLoadedResource(request,
                                   navigation_simulator->GetGlobalRequestID());

  NavigateMainFrame(kNonAdUrl);

  // We only log histograms if we observed bytes for the page. Verify that the
  // main frame resource was properly tracked and attributed.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 0, 1);
  // There shouldn't be any other histograms for a page with no ad
  // resources.
  EXPECT_EQ(1u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.Google.")
                    .size());
}

// Make sure that ads histograms aren't recorded if the tracker never commits
// (see https://crbug.com/723219).
TEST_F(AdsPageLoadMetricsObserverTest, NoHistogramWithoutCommit) {
  {
    // Once the metrics observer has the GlobalRequestID, throttle.
    content::TestNavigationThrottleInserter throttle_inserter(
        web_contents(),
        base::BindRepeating(&ResourceLoadingCancellingThrottle::Create));

    // Start main-frame navigation. The commit will defer after calling
    // WillProcessNavigationResponse, it will load a resource, and then the
    // throttle will cancel the commit.
    SimulateNavigateAndCommit(GURL(kNonAdUrl), main_rfh());
  }

  // Force navigation to a new page to make sure OnComplete() runs for the
  // previous failed navigation.
  NavigateMainFrame(kNonAdUrl);

  // There shouldn't be any histograms for an aborted main frame.
  EXPECT_EQ(0u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.Google.")
                    .size());
}

// Frames that are disallowed (and filtered) by the subresource filter should
// not be counted.
TEST_F(AdsPageLoadMetricsObserverTest, FilterAds_DoNotLogMetrics) {
  ConfigureAsSubresourceFilterOnlyURL(GURL(kNonAdUrl));
  NavigateMainFrame(kNonAdUrl);

  LoadResource(main_rfh(), ResourceCached::NOT_CACHED, 10);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild(kNonAdName);
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kDefaultDisallowedUrl),
                                                   subframe);
  LoadResource(subframe, ResourceCached::CACHED, 10);
  simulator->Commit();

  EXPECT_NE(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());

  NavigateMainFrame(kNonAdUrl);
  TestHistograms(histogram_tester(), std::vector<ExpectedFrameBytes>(),
                 0u /* non_ad_cached_kb */, 0u /* non_ad_uncached_kb */,
                 AdType::SUBRESOURCE_FILTER);
}

// UKM metrics for ad page load are recorded correctly.
TEST_F(AdsPageLoadMetricsObserverTest, AdPageLoadUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  NavigateMainFrame(kNonAdUrl);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::Now();
  timing.response_start = base::TimeDelta::FromSeconds(0);
  timing.interactive_timing->interactive = base::TimeDelta::FromSeconds(0);
  PopulateRequiredTimingFields(&timing);
  TimingUpdate(timing);
  ResourceDataUpdate(10 << 10 /* resource_size_in_kbyte */,
                     "application/javascript" /* mime_type */,
                     false /* is_ad_resource */);
  ResourceDataUpdate(10 << 10 /* resource_size_in_kbyte */,
                     "application/javascript" /* mime_type */,
                     true /* is_ad_resource */);
  ResourceDataUpdate(10 << 10 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);
  NavigateMainFrame(kNonAdUrl);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kTotalBytesName),
            30);
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            20);
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      10);
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            10);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdBytesPerSecondName),
      0);
  EXPECT_GT(
      *ukm_recorder.GetEntryMetric(
          entries.front(),
          ukm::builders::AdPageLoad::kAdBytesPerSecondAfterInteractiveName),
      0);
}

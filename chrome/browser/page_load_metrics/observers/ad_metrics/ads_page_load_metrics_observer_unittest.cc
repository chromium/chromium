// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_features.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "chrome/common/chrome_features.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "net/base/host_port_pair.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "url/gurl.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::TestNavigationThrottle;

namespace {

struct ExpectedFrameBytes {
  ExpectedFrameBytes(size_t cached_kb, size_t uncached_kb)
      : cached_kb(cached_kb), uncached_kb(uncached_kb) {}
  size_t cached_kb;
  size_t uncached_kb;

  bool operator<(const ExpectedFrameBytes& other) const {
    return cached_kb < other.cached_kb ||
           (cached_kb == other.cached_kb && uncached_kb < other.uncached_kb);
  }
};

enum class ResourceCached { kNotCached = 0, kCachedHttp, kCachedMemory };
enum class FrameType { AD = 0, NON_AD };

const char kAdUrl[] = "https://ads.com/ad/disallowed.html";
const char kNonAdUrl[] = "https://foo.com/";
const char kNonAdUrlSameOrigin[] = "https://ads.com/foo";

const int kMaxHeavyAdNetworkBytes =
    heavy_ad_thresholds::kMaxNetworkBytes +
    AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider::
        kMaxNetworkThresholdNoiseBytes;

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
    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    page_load_metrics::mojom::ResourceDataUpdatePtr resource =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource->received_data_length = 10 * 1024;
    resource->delta_bytes = 10 * 1024;
    resource->encoded_body_length = 10 * 1024;
    resource->cache_type = page_load_metrics::mojom::CacheType::kNotCached;
    resource->is_complete = true;
    resource->is_primary_frame_resource = true;
    resources.push_back(std::move(resource));
    auto timing = page_load_metrics::mojom::PageLoadTimingPtr(base::in_place);
    page_load_metrics::InitPageLoadTimingForTest(timing.get());
    observer->OnTimingUpdated(
        navigation_handle()->GetRenderFrameHost(), std::move(timing),
        page_load_metrics::mojom::PageLoadMetadataPtr(base::in_place),
        page_load_metrics::mojom::PageLoadFeaturesPtr(base::in_place),
        resources,
        page_load_metrics::mojom::FrameRenderDataUpdatePtr(base::in_place),
        page_load_metrics::mojom::CpuTimingPtr(base::in_place),
        page_load_metrics::mojom::DeferredResourceCountsPtr(base::in_place));
  }

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingCancellingThrottle);
};

// Mock noise provider which always gives a supplied value of noise for the
// heavy ad intervention thresholds.
class MockNoiseProvider
    : public AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider {
 public:
  explicit MockNoiseProvider(int noise)
      : HeavyAdThresholdNoiseProvider(true /* use_noise */), noise_(noise) {}
  ~MockNoiseProvider() override = default;

  int GetNetworkThresholdNoiseForFrame() const override { return noise_; }

 private:
  int noise_;
};

std::string SuffixedHistogram(const std::string& suffix) {
  return base::StringPrintf("PageLoad.Clients.Ads.%s", suffix.c_str());
}

// Verifies that the histograms match what is expected. Frames that should not
// be recorded (due to zero bytes and zero CPU usage) should be not be
// represented in |ad_frames|.
void TestHistograms(const base::HistogramTester& histograms,
                    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
                    const std::vector<ExpectedFrameBytes>& ad_frames,
                    size_t non_ad_cached_kb,
                    size_t non_ad_uncached_kb) {
  size_t total_ad_cached_kb = 0;
  size_t total_ad_uncached_kb = 0;
  size_t total_ad_kb = 0;
  size_t ad_frame_count = 0;

  std::map<size_t, int> frames_with_total_byte_count;
  std::map<size_t, int> frames_with_network_byte_count;
  std::map<size_t, int> frames_with_percent_network_count;

  // This map is keyed by (total bytes, network bytes).
  std::map<ExpectedFrameBytes, int> frame_byte_counts;

  // Perform some initial calculations on the number of bytes, of each type,
  // in each ad frame.
  for (const ExpectedFrameBytes& bytes : ad_frames) {
    total_ad_cached_kb += bytes.cached_kb;
    total_ad_uncached_kb += bytes.uncached_kb;
    total_ad_kb += bytes.cached_kb + bytes.uncached_kb;

    ad_frame_count += 1;

    size_t total_frame_kb = bytes.cached_kb + bytes.uncached_kb;

    frames_with_total_byte_count[total_frame_kb] += 1;
    frames_with_network_byte_count[bytes.uncached_kb] += 1;
    if (total_frame_kb > 0) {
      frames_with_percent_network_count[(bytes.uncached_kb * 100) /
                                        total_frame_kb] += 1;
    }
    frame_byte_counts[bytes] += 1;
  }

  // Test the histograms.
  histograms.ExpectUniqueSample(SuffixedHistogram("FrameCounts.AdFrames.Total"),
                                ad_frame_count, 1);

  if (ad_frame_count == 0)
    return;

  for (const auto& total_bytes_and_count : frames_with_total_byte_count) {
    histograms.ExpectBucketCount(
        SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"),
        total_bytes_and_count.first, total_bytes_and_count.second);
  }
  for (const auto& network_bytes_and_count : frames_with_network_byte_count) {
    histograms.ExpectBucketCount(
        SuffixedHistogram("Bytes.AdFrames.PerFrame.Network"),
        network_bytes_and_count.first, network_bytes_and_count.second);
  }
  for (const auto& percent_network_and_count :
       frames_with_percent_network_count) {
    histograms.ExpectBucketCount(
        SuffixedHistogram("Bytes.AdFrames.PerFrame.PercentNetwork2"),
        percent_network_and_count.first, percent_network_and_count.second);
  }

  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.Aggregate.Total2"), total_ad_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.Aggregate.Network"),
      total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.FullPage.Total2"),
      non_ad_cached_kb + non_ad_uncached_kb + total_ad_kb, 1);
  histograms.ExpectUniqueSample(SuffixedHistogram("Bytes.FullPage.Network"),
                                non_ad_uncached_kb + total_ad_uncached_kb, 1);
  histograms.ExpectUniqueSample(
      SuffixedHistogram("Bytes.NonAdFrames.Aggregate.Total2"),
      non_ad_cached_kb + non_ad_uncached_kb, 1);
  if (total_ad_kb + non_ad_cached_kb + non_ad_uncached_kb > 0) {
    histograms.ExpectUniqueSample(
        SuffixedHistogram("Bytes.FullPage.Total.PercentAds2"),
        (total_ad_kb * 100) /
            (total_ad_kb + non_ad_cached_kb + non_ad_uncached_kb),
        1);
  }
  if (total_ad_kb > 0) {
    histograms.ExpectUniqueSample(
        SuffixedHistogram("Bytes.AdFrames.Aggregate.PercentNetwork2"),
        ((total_ad_uncached_kb * 100) / total_ad_kb), 1);
  }
  if (total_ad_uncached_kb + non_ad_uncached_kb > 0) {
    histograms.ExpectUniqueSample(
        SuffixedHistogram("Bytes.FullPage.Network.PercentAds"),
        (total_ad_uncached_kb * 100) /
            (total_ad_uncached_kb + non_ad_uncached_kb),
        1);
  }

  // Verify AdFrameLoad UKM metrics.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(ad_frame_count, entries.size());

  for (const auto& byte_count : frame_byte_counts) {
    size_t cached_bytes = byte_count.first.cached_kb * 1024;
    size_t network_bytes = byte_count.first.uncached_kb * 1024;
    int matching_entries = 0;
    for (auto const* entry : entries) {
      int64_t entry_cache_bytes = *ukm_recorder.GetEntryMetric(
          entry, ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name);
      int64_t entry_network_bytes = *ukm_recorder.GetEntryMetric(
          entry, ukm::builders::AdFrameLoad::kLoading_NetworkBytesName);
      if (entry_cache_bytes ==
              ukm::GetExponentialBucketMinForBytes(cached_bytes) &&
          entry_network_bytes ==
              ukm::GetExponentialBucketMinForBytes(network_bytes))
        matching_entries++;
    }
    EXPECT_EQ(matching_entries, byte_count.second);
  }
}

// Waits for an error page for the heavy ad intervention to be navigated to.
class ErrorPageWaiter : public content::WebContentsObserver {
 public:
  explicit ErrorPageWaiter(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  ~ErrorPageWaiter() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (handle->GetNetErrorCode() != net::ERR_BLOCKED_BY_CLIENT) {
      is_error_page_ = false;
      return;
    }

    is_error_page_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  // Immediately returns if we are on an error page.
  void WaitForError() {
    if (is_error_page_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Returns if the last observed navigation was an error page.
  bool LastPageWasErrorPage() { return is_error_page_; }

 private:
  base::OnceClosure quit_closure_;
  bool is_error_page_ = false;
};

// Mock frame remote. Processes calls to SendInterventionReport and waits
// for all pending messages to be sent.
class FrameRemoteTester : public content::FakeLocalFrame {
 public:
  FrameRemoteTester() = default;
  ~FrameRemoteTester() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
                       std::move(handle)));
  }

  // blink::mojom::LocalFrame
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override {
    if (!on_empty_report_callback_)
      return;

    if (id.empty()) {
      std::move(on_empty_report_callback_).Run();
      return;
    }

    last_message_ = message;
    had_message_ = true;
  }

  // Sends an empty message and waits for it to be received. Returns true if any
  // other messages were received.
  bool FlushForTesting(RenderFrameHost* render_frame_host) {
    base::RunLoop run_loop;
    on_empty_report_callback_ = run_loop.QuitClosure();
    render_frame_host->SendInterventionReport("", "");
    run_loop.Run();
    int had_message = had_message_;
    had_message_ = false;
    return had_message;
  }

  // Returns the last observed report message and then clears it.
  std::string PopLastInterventionReportMessage() {
    std::string last_message = last_message_;
    last_message_ = "";
    return last_message;
  }

 private:
  bool had_message_ = false;

  // The message string for the last received non-empty intervention report.
  std::string last_message_;
  base::OnceClosure on_empty_report_callback_;
  mojo::AssociatedReceiverSet<blink::mojom::LocalFrame> receivers_;
};

}  // namespace

class AdsPageLoadMetricsObserverTest
    : public SubresourceFilterTestHarness,
      public blacklist::OptOutBlacklistDelegate {
 public:
  AdsPageLoadMetricsObserverTest()
      : test_blocklist_(std::make_unique<HeavyAdBlocklist>(
            nullptr,
            base::DefaultClock::GetInstance(),
            this)) {}

  void SetUp() override {
    SubresourceFilterTestHarness::SetUp();
    tester_ =
        std::make_unique<page_load_metrics::PageLoadMetricsObserverTester>(
            web_contents(), this,
            base::BindRepeating(
                &AdsPageLoadMetricsObserverTest::RegisterObservers,
                base::Unretained(this)));
    ConfigureAsSubresourceFilterOnlyURL(GURL(kAdUrl));
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

  // Frame creation doesn't trigger a mojo call since unit tests have no render
  // process. Just mock them for now.
  void OnAdSubframeDetected(RenderFrameHost* render_frame_host) {
    subresource_filter::SubresourceFilterObserverManager::FromWebContents(
        web_contents())
        ->NotifyAdSubframeDetected(render_frame_host);
  }

  // Set the interactive status of the main frame.
  void OnMainFrameInteractive(base::TimeDelta frame_interactive_offset) {
    auto timing = page_load_metrics::mojom::PageLoadTimingPtr(base::in_place);
    page_load_metrics::InitPageLoadTimingForTest(timing.get());
    timing->interactive_timing->interactive =
        base::Optional<base::TimeDelta>(frame_interactive_offset);
    // Call directly since main frame timing updates may be delayed.
    ads_observer_->OnPageInteractive(*timing);
  }

  void OnCpuTimingUpdate(RenderFrameHost* render_frame_host,
                         base::TimeDelta cpu_time_spent) {
    page_load_metrics::mojom::CpuTiming cpu_timing(cpu_time_spent);
    tester_->SimulateCpuTimingUpdate(cpu_timing, render_frame_host);
  }

  // Sends |total_time| in CPU timing updates spread across a variable amount of
  // 30 second windows to not hit the peak window usage cap for the heavy ad
  // intervention.
  void UseCpuTimeUnderThreshold(RenderFrameHost* render_frame_host,
                                base::TimeDelta total_time) {
    const base::TimeDelta peak_threshold = base::TimeDelta::FromMilliseconds(
        heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100 - 1);
    for (; total_time > peak_threshold; total_time -= peak_threshold) {
      OnCpuTimingUpdate(render_frame_host, peak_threshold);
      AdvancePageDuration(base::TimeDelta::FromSeconds(31));
    }
    OnCpuTimingUpdate(render_frame_host, total_time);
  }

  void OnHidden() { web_contents()->WasHidden(); }

  void OnShown() { web_contents()->WasShown(); }

  void TriggerFirstUserActivation(RenderFrameHost* render_frame_host) {
    tester_->SimulateFrameReceivedFirstUserActivation(render_frame_host);
  }

  void AdvancePageDuration(base::TimeDelta delta) { clock_->Advance(delta); }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             content::RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild("frame_name");
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), subframe);
    navigation_simulator->Commit();

    blink::AssociatedInterfaceProvider* remote_interfaces =
        navigation_simulator->GetFinalRenderFrameHost()
            ->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        blink::mojom::LocalFrame::Name_,
        base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                            base::Unretained(&frame_remote_tester_)));

    return navigation_simulator->GetFinalRenderFrameHost();
  }

  void ResourceDataUpdate(RenderFrameHost* render_frame_host,
                          ResourceCached resource_cached,
                          int resource_size_in_kbyte,
                          std::string mime_type = "",
                          bool is_ad_resource = false,
                          bool is_main_frame_resource = false) {
    std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
    page_load_metrics::mojom::ResourceDataUpdatePtr resource =
        page_load_metrics::mojom::ResourceDataUpdate::New();
    resource->received_data_length =
        static_cast<bool>(resource_cached) ? 0 : resource_size_in_kbyte << 10;
    resource->delta_bytes = resource->received_data_length;
    resource->encoded_body_length = resource_size_in_kbyte << 10;
    resource->reported_as_ad_resource = is_ad_resource;
    resource->is_complete = true;
    switch (resource_cached) {
      case ResourceCached::kNotCached:
        resource->cache_type = page_load_metrics::mojom::CacheType::kNotCached;
        break;
      case ResourceCached::kCachedHttp:
        resource->cache_type = page_load_metrics::mojom::CacheType::kHttp;
        break;
      case ResourceCached::kCachedMemory:
        resource->cache_type = page_load_metrics::mojom::CacheType::kMemory;
        break;
    }
    resource->mime_type = mime_type;
    resource->is_primary_frame_resource = true;
    resource->is_main_frame_resource =
        render_frame_host->GetFrameTreeNodeId() ==
        main_rfh()->GetFrameTreeNodeId();
    resources.push_back(std::move(resource));
    tester_->SimulateResourceDataUseUpdate(resources, render_frame_host);
  }

  void TimingUpdate(const page_load_metrics::mojom::PageLoadTiming& timing) {
    tester_->SimulateTimingUpdate(timing);
  }

  page_load_metrics::PageLoadMetricsObserverTester* tester() {
    return tester_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }

  HeavyAdBlocklist* blocklist() { return test_blocklist_.get(); }

  // Flushes all intervention report messages and returns a bool if there was a
  // message.
  bool HasInterventionReportsAfterFlush(RenderFrameHost* render_frame_host) {
    return frame_remote_tester_.FlushForTesting(render_frame_host);
  }

  std::string PopLastInterventionReportMessage() {
    return frame_remote_tester_.PopLastInterventionReportMessage();
  }

  void OverrideVisibilityTrackerWithMockClock() {
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    clock_->SetNowTicks(base::TimeTicks::Now());
  }

  void OverrideHeavyAdNoiseProvider(
      std::unique_ptr<MockNoiseProvider> noise_provider) {
    ads_observer_->SetHeavyAdThresholdNoiseProviderForTesting(
        std::move(noise_provider));
  }

  // Given the prefix of the CPU histogram to check, either "Cpu.FullPage" or
  // "Cpu.AdFrames.PerFrame", as well as the type, one of "" (for "FullPage"),
  // "Activated", or "Unactivated", along with the total pre and post cpu time
  // and total time, check all the relevant cpu histograms.
  void CheckCpuHistograms(const std::string& prefix,
                          std::string type,
                          int pre_task_time,
                          int pre_time,
                          int post_task_time,
                          int post_time) {
    int total_task_time = pre_task_time + post_task_time;
    int total_time = pre_time + post_time;
    std::string suffix = type == "Activated" ? "Activation" : "Interactive";
    type = type.empty() ? "" : "." + type;

    CheckSpecificCpuHistogram(SuffixedHistogram(prefix + ".TotalUsage" + type),
                              total_task_time, total_time);
    CheckSpecificCpuHistogram(
        SuffixedHistogram(prefix + ".TotalUsage" + type + ".Pre" + suffix),
        pre_task_time, pre_time);
    CheckSpecificCpuHistogram(
        SuffixedHistogram(prefix + ".TotalUsage" + type + ".Post" + suffix),
        post_task_time, post_time);
  }

 private:
  void CheckSpecificCpuHistogram(std::string total_histogram,
                                 int total_task_time,
                                 int total_time) {
    if (total_time) {
      histogram_tester().ExpectUniqueSample(total_histogram, total_task_time,
                                            1);
    } else {
      histogram_tester().ExpectTotalCount(total_histogram, 0);
    }
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    auto observer = std::make_unique<AdsPageLoadMetricsObserver>(
        clock_.get(), test_blocklist_.get());
    ads_observer_ = observer.get();

    // Mock the noise provider to make tests deterministic. Tests can override
    // this again to test non-zero noise.
    ads_observer_->SetHeavyAdThresholdNoiseProviderForTesting(
        std::make_unique<MockNoiseProvider>(0 /* noise */));
    tracker->AddObserver(std::move(observer));

    // Swap out the ui::ScopedVisibilityTracker to use the test clock.
    if (clock_) {
      ui::ScopedVisibilityTracker visibility_tracker(clock_.get(), true);
      tracker->SetVisibilityTrackerForTesting(visibility_tracker);
    }
  }

  std::unique_ptr<HeavyAdBlocklist> test_blocklist_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserverTester> tester_;
  FrameRemoteTester frame_remote_tester_;

  // The clock used by the ui::ScopedVisibilityTracker, assigned if non-null.
  std::unique_ptr<base::SimpleTestTickClock> clock_;

  // A pointer to the AdsPageLoadMetricsObserver used by the tests.
  AdsPageLoadMetricsObserver* ads_observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverTest);
};

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNoAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame2, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(),
                 std::vector<ExpectedFrameBytes>(), 0 /* non_ad_cached_kb */,
                 30 /* non_ad_uncached_kb */);

  // Verify that other UMA wasn't written.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Bytes.AdFrames.Aggregate.Total", 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame2, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, AdFrameMimeTypeBytes) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(
      ad_frame, ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     20 /* resource_size_in_kbyte */,
                     "image/png" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     30 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Cached resource not counted.
  ResourceDataUpdate(ad_frame, ResourceCached::kCachedHttp,
                     40 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_JavascriptBytesName,
      ukm::GetExponentialBucketMinForBytes(10 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_ImageBytesName,
      ukm::GetExponentialBucketMinForBytes(20 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_VideoBytesName,
      ukm::GetExponentialBucketMinForBytes(30 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NetworkBytesName,
      ukm::GetExponentialBucketMinForBytes(60 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_CacheBytes2Name,
      ukm::GetExponentialBucketMinForBytes(40 * 1024));
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kLoading_NumResourcesName,
      4);
}

TEST_F(AdsPageLoadMetricsObserverTest, ResourceBeforeAdFrameCommits) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Create subframe and load resource before commit.
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_frame)->AppendChild("foo");
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10);
  navigation_simulator->Commit();

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}},
                 0 /* non_ad_cached_kb */, 10 /*non_ad_uncached_kb*/);
}

// Test that the cross-origin ad subframe navigation metric works as it's
// supposed to, triggering a false addition with each ad that's in the same
// origin as the main page, and a true when when the ad has a separate origin.
TEST_F(AdsPageLoadMetricsObserverTest, AdsOriginStatusMetrics) {
  const char kCrossOriginHistogramId[] =
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
      "OriginStatus";

  // Test that when the main frame origin is different from a direct ad
  // subframe it is correctly identified as cross-origin, but do not count
  // indirect ad subframes.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    RenderFrameHost* ad_sub_frame =
        CreateAndNavigateSubFrame(kAdUrl, main_frame);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(ad_sub_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, ad_sub_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId,
                                  FrameData::OriginStatus::kCross, 1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(FrameData::OriginStatus::kCross));
  }

  // Add a non-ad subframe and an ad subframe and make sure the total count
  // only adjusts by one.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kNonAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId,
                                  FrameData::OriginStatus::kCross, 1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(FrameData::OriginStatus::kCross));
  }

  // Add an ad subframe in the same origin as the parent frame and make sure it
  // gets identified as non-cross-origin. Note: top-level navigations are never
  // considered to be ads.
  {
    base::HistogramTester histograms;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrlSameOrigin);
    ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
    ResourceDataUpdate(CreateAndNavigateSubFrame(kAdUrl, main_frame),
                       ResourceCached::kNotCached, 10);
    // Trigger histograms by navigating away, then test them.
    NavigateFrame(kAdUrl, main_frame);
    histograms.ExpectUniqueSample(kCrossOriginHistogramId,
                                  FrameData::OriginStatus::kSame, 1);
    auto entries =
        ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder.ExpectEntryMetric(
        entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
        static_cast<int64_t>(FrameData::OriginStatus::kSame));
  }
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithAdFrameThatRenavigates) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Navigate the ad frame again.
  ad_frame = NavigateFrame(kNonAdUrl, ad_frame);

  // In total, 30KB for entire page and 20 in one ad frame.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNonAdFrameThatRenavigatesToAd) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);

  // Child of the sub-frame that is an ad.
  RenderFrameHost* sub_frame_child_ad =
      CreateAndNavigateSubFrame(kAdUrl, sub_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame_child_ad, ResourceCached::kNotCached, 10);

  // Navigate the subframe again, this time it's an ad.
  sub_frame = NavigateFrame(kAdUrl, sub_frame);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // In total, 40KB was loaded for the entire page and 20KB from ad
  // frames (the original child ad frame and the renavigated frame which
  // turned into an ad).

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 10}, {0, 10}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedNavigation) {
  // If the first navigation in a frame is aborted, keep track of its bytes.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Create an ad subframe that aborts before committing.
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild("foo");
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), subframe_ad);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  OnAdSubframeDetected(subframe_ad);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). They should
  // be counted.
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedSecondNavigationForFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // Now navigate (and abort) the subframe to an ad.
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), sub_frame);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  OnAdSubframeDetected(sub_frame);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). Since the
  // frame attempted to load an ad, the frame is tagged forever as an ad.
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(sub_frame, ResourceCached::kNotCached, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 20 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, TwoResourceLoadsBeforeCommit) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Now open a subframe and have its resource load before notification of
  // navigation finishing.
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild("foo");
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), subframe_ad);
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);

  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  OnAdSubframeDetected(subframe_ad);
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Renavigate the subframe to a successful commit. But again, the resource
  // loads before the observer sees the finished navigation.
  ResourceDataUpdate(subframe_ad, ResourceCached::kNotCached, 10);
  NavigateFrame(kNonAdUrl, subframe_ad);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

TEST_F(AdsPageLoadMetricsObserverTest, MainFrameResource) {
  // Start main-frame navigation
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), web_contents()->GetMainFrame());
  navigation_simulator->Start();
  navigation_simulator->Commit();

  ResourceDataUpdate(navigation_simulator->GetFinalRenderFrameHost(),
                     ResourceCached::kNotCached, 10);

  NavigateMainFrame(kNonAdUrl);

  // We only log histograms if we observed bytes for the page. Verify that the
  // main frame resource was properly tracked and attributed.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.FrameCounts.AdFrames.Total", 0, 1);

  // Verify that this histogram is also recorded for the Visible and NonVisible
  // suffixes.
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Visible.FrameCounts.AdFrames.Total", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.NonVisible.FrameCounts.AdFrames.Total", 1);

  // There are three FrameCounts.AdFrames.Total histograms
  // recorded for each page load, one for each visibility type. There shouldn't
  // be any other histograms for a page with no ad resources.
  EXPECT_EQ(3u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.")
                    .size());
  EXPECT_EQ(0u, test_ukm_recorder()
                    .GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName)
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
                    .GetTotalCountsForPrefix("PageLoad.Clients.Ads.")
                    .size());
  EXPECT_EQ(0u, test_ukm_recorder()
                    .GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName)
                    .size());
}

// Frames that are disallowed (and filtered) by the subresource filter should
// not be counted.
TEST_F(AdsPageLoadMetricsObserverTest, FilterAds_DoNotLogMetrics) {
  ConfigureAsSubresourceFilterOnlyURL(GURL(kNonAdUrl));
  NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached, 10,
                     "" /* mime_type */, false /* is_ad_resource */);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("foo");
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kDefaultDisallowedUrl),
                                                   subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  simulator->Commit();

  EXPECT_NE(content::NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult());

  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);
}

// Per-frame histograms recorded when root ad frame is destroyed.
TEST_F(AdsPageLoadMetricsObserverTest,
       FrameDestroyed_PerFrameHistogramsLogged) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* child_ad_frame = CreateAndNavigateSubFrame(kAdUrl, ad_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(child_ad_frame, ResourceCached::kNotCached, 10);

  // Just delete the child frame this time.
  content::RenderFrameHostTester::For(child_ad_frame)->Detach();

  // Verify per-frame histograms not recorded.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"), 0);

  // Delete the root ad frame.
  content::RenderFrameHostTester::For(ad_frame)->Detach();

  // Verify per-frame histograms are recorded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.AdFrames.PerFrame.Total2"), 20, 1);

  // Verify page totals not reported yet.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);

  NavigateMainFrame(kNonAdUrl);

  // Verify histograms are logged correctly for the whole page.
  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 20}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

// Tests that a non ad frame that is deleted does not cause any unspecified
// behavior (see https://crbug.com/973954).
TEST_F(AdsPageLoadMetricsObserverTest, NonAdFrameDestroyed_FrameDeleted) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* vanilla_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  content::RenderFrameHostTester::For(vanilla_frame)->Detach();

  NavigateMainFrame(kNonAdUrl);
}

// Tests that main frame ad bytes are recorded correctly.
TEST_F(AdsPageLoadMetricsObserverTest, MainFrameAdBytesRecorded) {
  NavigateMainFrame(kNonAdUrl);

  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(main_rfh(), ResourceCached::kCachedHttp, 10,
                     "" /* mime_type */, true /* is_ad_resource */);

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("foo");
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kDefaultDisallowedUrl),
                                                   subframe);
  ResourceDataUpdate(subframe, ResourceCached::kNotCached, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(subframe, ResourceCached::kCachedHttp, 10,
                     "" /* mime_type */, true /* is_ad_resource */);
  simulator->Commit();

  NavigateMainFrame(kNonAdUrl);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.MainFrame.Ads.Total2"), 20, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Bytes.MainFrame.Ads.Network"), 10, 1);

  // Verify page total for network bytes.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Resources.Bytes.Ads2"), 20, 1);

  // Verify main frame ad bytes recorded in UKM.
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kMainframeAdBytesName),
      ukm::GetExponentialBucketMinForBytes(10 * 1024));
}

// Tests that memory cache ad bytes are recorded correctly.
TEST_F(AdsPageLoadMetricsObserverTest, MemoryCacheAdBytesRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 = CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* frame2 = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);
  ResourceDataUpdate(frame1, ResourceCached::kCachedMemory, 10);
  ResourceDataUpdate(frame2, ResourceCached::kCachedMemory, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  TestHistograms(histogram_tester(), test_ukm_recorder(), {{10, 0}},
                 10 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);
}

// UKM metrics for ad page load are recorded correctly.
TEST_F(AdsPageLoadMetricsObserverTest, AdPageLoadUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::Now();
  timing.response_start = base::TimeDelta::FromSeconds(0);
  timing.interactive_timing->interactive = base::TimeDelta::FromSeconds(0);
  PopulateRequiredTimingFields(&timing);
  TimingUpdate(timing);
  ResourceDataUpdate(
      main_rfh(), ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, false /* is_ad_resource */);
  ResourceDataUpdate(
      main_rfh(), ResourceCached::kNotCached, 10 /* resource_size_in_kbyte */,
      "application/javascript" /* mime_type */, true /* is_ad_resource */);
  ResourceDataUpdate(main_rfh(), ResourceCached::kNotCached,
                     10 /* resource_size_in_kbyte */,
                     "video/webm" /* mime_type */, true /* is_ad_resource */);

  // Update cpu timings.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(main_rfh(), base::TimeDelta::FromMilliseconds(500));
  NavigateMainFrame(kNonAdUrl);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kTotalBytesName),
            30);
  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdBytesName),
            20);
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdJavascriptBytesName),
      10);
  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdVideoBytesName),
            10);
  EXPECT_GT(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kAdBytesPerSecondName),
      0);
  EXPECT_GT(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(),
          ukm::builders::AdPageLoad::kAdBytesPerSecondAfterInteractiveName),
      0);
  EXPECT_EQ(
      *test_ukm_recorder().GetEntryMetric(
          entries.front(), ukm::builders::AdPageLoad::kMainframeAdBytesName),
      ukm::GetExponentialBucketMinForBytes(20 * 1024));
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                entries.front(), ukm::builders::AdPageLoad::kAdCpuTimeName),
            500);
}

TEST_F(AdsPageLoadMetricsObserverTest, ZeroBytesZeroCpuUseFrame_NotRecorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  CreateAndNavigateSubFrame(kAdUrl, main_frame);

  NavigateFrame(kNonAdUrl, main_frame);

  // We expect frames with no bytes and no CPU usage to be ignored
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("FrameCounts.AdFrames.Total"), 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, ZeroBytesNonZeroCpuFrame_Recorded) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ResourceDataUpdate(main_frame, ResourceCached::kNotCached, 10);

  // Use CPU but maintain zero bytes in the ad frame
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  NavigateFrame(kNonAdUrl, main_frame);

  // We expect the frame to be recorded as it has non-zero CPU usage
  TestHistograms(histogram_tester(), test_ukm_recorder(), {{0, 0}},
                 0 /* non_ad_cached_kb */, 10 /* non_ad_uncached_kb */);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.TotalUsage"), 1000, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsWindowed) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames. Usage 1%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Advance time by twelve seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Do some more work on the ad frame. Usage 5%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Do some more work on the ad frame. Usage 8%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Perform some updates on ad and non-ad frames. Usage 10%/13%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Perform some updates on ad and non-ad frames. Usage 8%/11%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away and check the peak windowed cpu usage.
  NavigateFrame(kNonAdUrl, main_frame);

  // 10% is the maximum for the individual ad frame.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent"), 10, 1);

  // The peak window started at 12 seconds into the page load
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowStartTime"), 12000, 1);

  // 13% is the maximum for all frames (including main).
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent"), 13, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowStartTime"), 12000, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsWindowedActivated) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames. Usage 1%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Advance time by twelve seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Do some more work on the ad frame. Usage 8%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(2000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Do some more work on the ad frame. Usage 11%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Set the page activation and advance time by twelve more seconds.
  TriggerFirstUserActivation(ad_frame);
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Perform some updates on ad and main frames. Usage 13%/16%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(1000));

  // Advance time by twelve more seconds.
  AdvancePageDuration(base::TimeDelta::FromSeconds(12));

  // Perform some updates on ad and non-ad frames. Usage 8%/11%.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away and check the peak windowed cpu usage.
  NavigateFrame(kNonAdUrl, main_frame);

  // 11% is the maximum before activation for the ad frame.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent"), 11, 1);

  // The peak window started at 0 seconds into the page load
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowStartTime"), 0, 1);

  // 16% is the maximum for all frames (including main), ignores activation.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent"), 16, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.FullPage.PeakWindowStartTime"), 12000, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetrics) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it gets reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set the main frame as interactive after 2 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  OnMainFrameInteractive(base::TimeDelta::FromMilliseconds(2000));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Do some more work on the main frame.
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckCpuHistograms("Cpu.FullPage", "", /*pre_tasks=*/500 + 500,
                     /*pre_time=*/2000, /*post_tasks=*/1000 + 500,
                     /*post_time=*/2000);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", /*pre_tasks=*/500,
                     /*pre_time=*/2000, /*post_tasks=*/1000,
                     /*post_time=*/2000);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"),
      /*total_task_time=*/500 + 1000, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1500 / 30000);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_PreActivationForegroundDurationName));
}

TEST_F(AdsPageLoadMetricsObserverTest,
       TestCpuTimingMetricsStopWhenBackgrounded) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set the main frame as interactive after 2 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  OnMainFrameInteractive(base::TimeDelta::FromMilliseconds(2000));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Set the page as hidden after 3.5 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(1500));
  OnHidden();

  // Do some more work on the main frame, shouldn't count to total.
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(500));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckCpuHistograms("Cpu.FullPage", "", /*pre_tasks=*/500 + 500,
                     /*pre_time=*/2000, /*post_tasks=*/1000,
                     /*post_time=*/1500);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", /*pre_tasks=*/500,
                     /*pre_time=*/2000, /*post_tasks=*/1000,
                     /*post_time=*/1500);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"),
      /*total_task_time=*/500 + 1000, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1500 / 30000);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_PreActivationForegroundDurationName));
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsOnActivation) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set the main frame as interactive after 2 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  OnMainFrameInteractive(base::TimeDelta::FromMilliseconds(2000));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set the frame as interactive after 2.5 seconds
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(500));
  TriggerFirstUserActivation(ad_frame);

  // Do some more work on the main frame.
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(500));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(1500));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckCpuHistograms("Cpu.FullPage", "", /*pre_tasks=*/500 + 500,
                     /*pre_time=*/2000, /*post_tasks=*/1000 + 500,
                     /*post_time=*/2000);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated",
                     /*pre_tasks=*/500 + 500, /*pre_time=*/2500,
                     /*post_tasks=*/500,
                     /*post_time=*/1500);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"),
      /*total_task_time=*/1000 + 500, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1000 / 30000);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName,
      1000);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_PreActivationForegroundDurationName,
      2500);
}

TEST_F(AdsPageLoadMetricsObserverTest, TestNoReportingWhenAlwaysBackgrounded) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Set the frame as backgrounded, so all updates below shouldn't report.
  OnHidden();

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set the main frame as interactive after 2 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  OnMainFrameInteractive(base::TimeDelta::FromMilliseconds(2000));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Do some more work on the main frame.
  OnCpuTimingUpdate(main_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 4 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  NavigateFrame(kNonAdUrl, main_frame);

  // Ensure that all metrics are zero.
  CheckCpuHistograms("Cpu.FullPage", "", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated", 0, 0, 0, 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowStartTime"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.PeakWindowStartTime"), 0);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 0);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName, 0);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_PreActivationName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_PreActivationForegroundDurationName));
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsNoInteractive) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 2 seconds.
  AdvancePageDuration(base::TimeDelta::FromMilliseconds(2000));
  NavigateFrame(kNonAdUrl, main_frame);

  // Check the cpu histograms.
  CheckCpuHistograms("Cpu.FullPage", "", /*pre_tasks=*/500 + 500,
                     /*pre_time=*/2000, /*post_tasks=*/0, /*post_time=*/0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", /*pre_tasks=*/500,
                     /*pre_time=*/2000, /*post_tasks=*/0, /*post_time=*/0);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"),
      /*total_task_time=*/500, 1);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 500 / 30000);
}

TEST_F(AdsPageLoadMetricsObserverTest, TestCpuTimingMetricsShortTimeframes) {
  OverrideVisibilityTrackerWithMockClock();
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, main_frame);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  // Perform some updates on ad and non-ad frames.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Set interactive after 1 microsecond.
  AdvancePageDuration(base::TimeDelta::FromMicroseconds(1));
  OnMainFrameInteractive(base::TimeDelta::FromMicroseconds(1));

  // Do some more work on the ad frame.
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1000));

  // Do some more work on the main frame.
  OnCpuTimingUpdate(non_ad_frame, base::TimeDelta::FromMilliseconds(500));

  // Navigate away after 2 microseconds.
  AdvancePageDuration(base::TimeDelta::FromMicroseconds(1));
  NavigateFrame(kNonAdUrl, main_frame);

  // Make sure there are no numbers reported, as the timeframes are too short.
  CheckCpuHistograms("Cpu.FullPage", "", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Activated", 0, 0, 0, 0);
  CheckCpuHistograms("Cpu.AdFrames.PerFrame", "Unactivated", 0, 0, 0, 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowedPercent"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.PerFrame.PeakWindowStartTime"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.AdFrames.Aggregate.TotalUsage"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.PeakWindowedPercent"), 0);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("Cpu.FullPage.PeakWindowStartTime"), 0);

  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kCpuTime_TotalName, 1500);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kCpuTime_PeakWindowedPercentName,
      100 * 1500 / 30000);
}

TEST_F(AdsPageLoadMetricsObserverTest, AdFrameLoadTiming) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load bytes in frame to record ukm event.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 10);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(0);
  subframe_timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(0);
  PopulateRequiredTimingFields(&subframe_timing);
  tester()->SimulateTimingUpdate(subframe_timing, ad_frame);

  // Send an updated timing that should be recorded.
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(5);
  subframe_timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(20);
  PopulateRequiredTimingFields(&subframe_timing);
  tester()->SimulateTimingUpdate(subframe_timing, ad_frame);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);
  auto entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdFrameLoad::kTiming_FirstContentfulPaintName, 5);
  test_ukm_recorder().ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kTiming_InteractiveName, 20);
}

// Tests that even when the intervention is not enabled, we still record the
// computed heavy ad types for ad frames
TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdFeatureOff_UMARecorded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHeavyAdIntervention);
  OverrideVisibilityTrackerWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame_none =
      CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* ad_frame_net = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* ad_frame_cpu = CreateAndNavigateSubFrame(kAdUrl, main_frame);
  RenderFrameHost* ad_frame_total_cpu =
      CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load some bytes in each frame so they are considered ad iframes.
  ResourceDataUpdate(ad_frame_none, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_net, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_cpu, ResourceCached::kNotCached, 1);
  ResourceDataUpdate(ad_frame_total_cpu, ResourceCached::kNotCached, 1);

  // Make three of the ad frames hit thresholds for heavy ads.
  ResourceDataUpdate(ad_frame_net, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024));
  OnCpuTimingUpdate(
      ad_frame_cpu,
      base::TimeDelta::FromMilliseconds(
          heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100));
  UseCpuTimeUnderThreshold(
      ad_frame_total_cpu,
      base::TimeDelta::FromMilliseconds(heavy_ad_thresholds::kMaxCpuTime));

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.ComputedType2"), 4);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kNone, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kPeakCpu, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kTotalCpu, 1);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"), 4);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kNone, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kPeakCpu, 1);
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kTotalCpu, 1);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);

  // There were heavy ads on the page and the page was navigated not reloaded.
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.UserDidReload"), false, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdNetworkUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load just under the threshold amount of bytes.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) - 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 2);

  const char kInterventionMessage[] =
      "Ad was removed because its network usage exceeded the limit. "
      "See https://www.chromestatus.com/feature/4800491902992384";
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  EXPECT_EQ(kInterventionMessage, PopLastInterventionReportMessage());

  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageWithNoise_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load just under the threshold amount of bytes with noise included.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to meet the noised threshold criteria.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), false, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageLessThanNoisedThreshold_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load network bytes that trip the heavy ad threshold without noise.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     heavy_ad_thresholds::kMaxNetworkBytes / 1024 + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kNone, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdNetworkUsageLessThanNoisedThreshold_CpuTriggers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);
  OverrideVisibilityTrackerWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  OverrideHeavyAdNoiseProvider(
      std::make_unique<MockNoiseProvider>(2048 /* network noise */));
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Load network bytes that trip the heavy ad threshold without noise.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     heavy_ad_thresholds::kMaxNetworkBytes / 1024 + 1);
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.InterventionType2"), 0);

  // Verify the frame can still trip the CPU threshold.
  UseCpuTimeUnderThreshold(ad_frame, base::TimeDelta::FromMilliseconds(
                                         heavy_ad_thresholds::kMaxCpuTime + 1));

  // Verify we did not trigger the intervention.
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kTotalCpu, 1);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kTotalCpu, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdTotalCpuUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);
  OverrideVisibilityTrackerWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  // Use just under the threshold amount of CPU.Needs to spread across enough
  // windows to not trigger peak threshold.
  AdvancePageDuration(base::TimeDelta::FromSeconds(30));
  UseCpuTimeUnderThreshold(ad_frame, base::TimeDelta::FromMilliseconds(
                                         heavy_ad_thresholds::kMaxCpuTime - 1));

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  AdvancePageDuration(base::TimeDelta::FromSeconds(30));

  // Use enough CPU to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1));

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kTotalCpu, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdPeakCpuUsage_InterventionFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);
  OverrideVisibilityTrackerWithMockClock();

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add some data to the ad frame so it get reported.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached, 1);

  // Use just under the peak threshold amount of CPU.
  OnCpuTimingUpdate(
      ad_frame,
      base::TimeDelta::FromMilliseconds(
          heavy_ad_thresholds::kMaxPeakWindowedPercent * 30000 / 100 - 1));

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Use enough CPU to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  AdvancePageDuration(base::TimeDelta::FromSeconds(10));
  OnCpuTimingUpdate(ad_frame, base::TimeDelta::FromMilliseconds(1));

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kPeakCpu, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdFeatureDisabled_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (kMaxHeavyAdNetworkBytes / 1024) + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), 0);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdWithUserGesture_NotConsideredHeavy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Give the frame a user activation before the threshold would be hit.
  TriggerFirstUserActivation(ad_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedType2"),
      FrameData::HeavyAdStatus::kNone, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdPageNavigated_FrameMarkedAsNotRemoved) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.FrameRemovedPriorToPageEnd"), false, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdFrameRemoved_FrameMarkedAsRemoved) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Delete the root ad frame.
  content::RenderFrameHostTester::For(ad_frame)->Detach();

  NavigateMainFrame(kNonAdUrl);

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.FrameRemovedPriorToPageEnd"), true, 1);
}

// Verifies when a user reloads a page with a heavy ad we log it to metrics.
TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdPageReload_MetricsRecorded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Reload the page.
  NavigationSimulator::Reload(web_contents());

  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.ComputedTypeWithThresholdNoise"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.UserDidReload"), true, 1);
}

// Verifies when there is no heavy ad on the page, we do not record aggregate
// heavy ad metrics.
TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdsNoHeavyAdFrame_AggregateHistogramsNotRecorded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Don't load enough to reach the heavy ad threshold.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) - 1);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.UserDidReload"), 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdBlocklistFull_NotFired) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  // Five interventions are allowed to occur, per origin per day. Add five
  // entries to the blocklist.
  for (int i = 0; i < 5; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify we did not trigger the intervention.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdBlocklistDisabled_InterventionNotBlocked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kHeavyAdIntervention},
                                {features::kHeavyAdPrivacyMitigations});

  // Fill up the blocklist to verify the blocklist logic is correctly ignored
  // when disabled.
  for (int i = 0; i < 5; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);

  // This histogram should not be recorded when the blocklist is disabled.
  histogram_tester().ExpectTotalCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdBlocklist_InterventionReported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHeavyAdIntervention);

  // Five interventions are allowed to occur, per origin per day. Add four
  // entries to the blocklist.
  for (int i = 0; i < 4; i++)
    blocklist()->AddEntry(GURL(kNonAdUrl).host(), true, 0);

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ErrorPageWaiter waiter(web_contents());
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify the intervention triggered.
  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  waiter.WaitForError();
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), false, 1);

  // Verify the blocklist blocks the next intervention.
  ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  // Add enough data to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  // Verify the intervention did not occur again.
  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));
  histogram_tester().ExpectBucketCount(
      SuffixedHistogram("HeavyAds.DisallowedByBlocklist"), true, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest,
       HeavyAdReportingOnly_ReportSentNoUnload) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kHeavyAdIntervention,
        {{kHeavyAdReportingOnlyParamName, "true"}}}},
      {});

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  const char kReportOnlyMessage[] =
      "A future version of Chrome will remove this ad because its network "
      "usage exceeded the limit. "
      "See https://www.chromestatus.com/feature/4800491902992384";

  EXPECT_TRUE(HasInterventionReportsAfterFlush(ad_frame));
  EXPECT_EQ(kReportOnlyMessage, PopLastInterventionReportMessage());

  // It is not ideal to check the last loaded page here as it requires relying
  // on mojo timings after flushing the interface above. But the ordering is
  // deterministic as intervention reports and navigation use the same mojo
  // pipe.
  EXPECT_FALSE(waiter.LastPageWasErrorPage());
  histogram_tester().ExpectUniqueSample(
      SuffixedHistogram("HeavyAds.InterventionType2"),
      FrameData::HeavyAdStatus::kNetwork, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, HeavyAdReportingDisabled_NoReportSent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kHeavyAdIntervention,
        {{kHeavyAdReportingEnabledParamName, "false"}}}},
      {});

  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame = CreateAndNavigateSubFrame(kAdUrl, main_frame);

  ErrorPageWaiter waiter(web_contents());

  // Load enough bytes to trigger the intervention.
  ResourceDataUpdate(ad_frame, ResourceCached::kNotCached,
                     (heavy_ad_thresholds::kMaxNetworkBytes / 1024) + 1);

  EXPECT_FALSE(HasInterventionReportsAfterFlush(ad_frame));

  waiter.WaitForError();
}

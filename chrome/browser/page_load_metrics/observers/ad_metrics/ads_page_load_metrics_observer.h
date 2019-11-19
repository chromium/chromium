// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"

class HeavyAdBlocklist;

// This observer labels each sub-frame as an ad or not, and keeps track of
// relevant per-frame and whole-page byte statistics.
class AdsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  // Returns a new AdsPageLoadMetricObserver. If the feature is disabled it
  // returns nullptr.
  static std::unique_ptr<AdsPageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents);

  // For a given subframe, returns whether or not the subframe's url would be
  // considering same origin to the main frame's url. |use_parent_origin|
  // indicates that the subframe's parent frames's origin should be used when
  // performing the comparison.
  static bool IsSubframeSameOriginToMainFrame(
      content::RenderFrameHost* sub_host,
      bool use_parent_origin);

  using ResourceMimeType = FrameData::ResourceMimeType;

  // Aggregates high level summary statistics across FrameData objects.
  struct AggregateFrameInfo {
    AggregateFrameInfo();
    size_t bytes;
    size_t network_bytes;
    size_t num_frames;
    base::TimeDelta cpu_time;

    DISALLOW_COPY_AND_ASSIGN(AggregateFrameInfo);
  };

  // Helper class that generates a random amount of noise to apply to thresholds
  // for heavy ads. A different noise should be generated for each frame.
  class HeavyAdThresholdNoiseProvider {
   public:
    // |use_noise| indicates whether this provider should give values of noise
    // or just 0. If the heavy ad blocklist mitigation is disabled, |use_noise|
    // should be set to false to provide a deterministic debugging path.
    explicit HeavyAdThresholdNoiseProvider(bool use_noise);
    virtual ~HeavyAdThresholdNoiseProvider() = default;

    // Gets a random amount of noise to add to a threshold. The generated noise
    // is uniform random over the range 0 to kMaxThresholdNoiseBytes. Virtual
    // for testing.
    virtual int GetNetworkThresholdNoiseForFrame() const;

    // Maximum amount of additive noise to add to the network threshold to
    // obscure cross origin resource sizes: 1303 KB.
    static const int kMaxNetworkThresholdNoiseBytes = 1303 * 1024;

   private:
    // Whether to use noise.
    const bool use_noise_;
  };

  explicit AdsPageLoadMetricsObserver(base::TickClock* clock = nullptr,
                                      HeavyAdBlocklist* blocklist = nullptr);
  ~AdsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;
  void RecordAdFrameData(FrameTreeNodeId ad_id,
                         bool is_adframe,
                         content::RenderFrameHost* ad_host,
                         bool frame_navigated);
  void ReadyToCommitNextNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnPageInteractive(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void FrameReceivedFirstUserActivation(content::RenderFrameHost* rfh) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override;
  void OnFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void SetHeavyAdThresholdNoiseProviderForTesting(
      std::unique_ptr<HeavyAdThresholdNoiseProvider> noise_provider) {
    heavy_ad_threshold_noise_provider_ = std::move(noise_provider);
  }

 private:
  // subresource_filter::SubresourceFilterObserver:
  void OnAdSubframeDetected(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubresourceFilterGoingAway() override;
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::mojom::ActivationState& activation_state)
      override;

  // Gets the number of bytes that we may have not attributed to ad
  // resources due to the resource being reported as an ad late.
  int GetUnaccountedAdBytes(
      int process_id,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) const;

  // Updates page level counters for resource loads.
  void ProcessResourceForPage(
      int process_id,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);
  void ProcessResourceForFrame(
      content::RenderFrameHost* render_frame_host,
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  void RecordPageResourceTotalHistograms(ukm::SourceId source_id);
  void RecordHistograms(ukm::SourceId source_id);
  void RecordAggregateHistogramsForCpuUsage();
  void RecordAggregateHistogramsForAdTagging(
      FrameData::FrameVisibility visibility);
  void RecordAggregateHistogramsForHeavyAds();

  // Should be called on all frames prior to recording any aggregate histograms.
  void RecordPerFrameHistograms(const FrameData& ad_frame_data);
  void RecordPerFrameHistogramsForAdTagging(const FrameData& ad_frame_data);
  void RecordPerFrameHistogramsForCpuUsage(const FrameData& ad_frame_data);
  void RecordPerFrameHistogramsForHeavyAds(const FrameData& ad_frame_data);

  // Checks to see if a resource is waiting for a navigation in the given
  // RenderFrameHost to commit before it can be processed. If so, call
  // OnResourceDataUpdate for the delayed resource.
  void ProcessOngoingNavigationResource(content::RenderFrameHost* rfh);

  // Find the FrameData object associated with a given FrameTreeNodeId in
  // |ad_frames_data_storage_|.
  FrameData* FindFrameData(FrameTreeNodeId id);

  // Triggers the heavy ad intervention page in the target frame if it is safe
  // to do so on this origin, and the frame meets the criteria to be considered
  // a heavy ad. This first sends an intervention report to every affected
  // frame then loads an error page in the root ad frame.
  void MaybeTriggerHeavyAdIntervention(
      content::RenderFrameHost* render_frame_host,
      FrameData* frame_data);

  bool IsBlocklisted();
  HeavyAdBlocklist* GetHeavyAdBlocklist();
  void RecordHeavyAdInterventionDisallowedByBlocklist(bool disallowed);

  // Stores the size data of each ad frame. Pointed to by ad_frames_ so use a
  // data structure that won't move the data around. This only stores ad frames
  // that are actively on the page. When a frame is destroyed, so should its
  // FrameData.
  std::list<FrameData> ad_frames_data_storage_;

  // Maps a frame (by id) to the corresponding iterator of
  // |ad_frames_data_storage_| responsible for the frame. Multiple frame ids can
  // point to the same FrameData. The responsible frame is the top-most frame
  // labeled as an ad in the frame's ancestry, which may be itself. If no
  // responsible frame is found, the data is an iterator to the end of
  // |ad_frames_data_storage_|.
  std::map<FrameTreeNodeId, std::list<FrameData>::iterator> ad_frames_data_;

  int64_t navigation_id_ = -1;
  bool subresource_filter_is_enabled_ = false;

  // When the observer receives report of a document resource loading for a
  // sub-frame before the sub-frame commit occurs, hold onto the resource
  // request info (delay it) until the sub-frame commits.
  std::map<FrameTreeNodeId, page_load_metrics::mojom::ResourceDataUpdatePtr>
      ongoing_navigation_resources_;

  // Tracks byte counts only for resources loaded in the main frame.
  std::unique_ptr<FrameData> main_frame_data_;

  // Tracks aggregate counts across all frames on the page.
  std::unique_ptr<FrameData> aggregate_frame_data_;

  // Tracks aggregate counts across all ad frames on the page by visibility
  // type.
  AggregateFrameInfo aggregate_ad_info_by_visibility_
      [static_cast<size_t>(FrameData::FrameVisibility::kMaxValue) + 1];

  // Flag denoting that this observer should no longer monitor changes in
  // display state for frames. This prevents us from receiving the updates when
  // the frame elements are being destroyed in the renderer.
  bool process_display_state_updates_ = true;

  // Time the page was committed.
  base::TimeTicks time_commit_;

  // Time the page was observed to be interactive.
  base::TimeTicks time_interactive_;

  // Duration before |time_interactive_| during which the page was foregrounded.
  base::TimeDelta pre_interactive_duration_;

  // Total ad bytes loaded by the page since it was observed to be interactive.
  size_t page_ad_bytes_at_interactive_ = 0u;

  bool committed_ = false;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      subresource_observer_;

  // The tick clock used to get the current time.  Can be replaced by tests.
  const base::TickClock* clock_;

  // Stores whether the heavy ad intervention is blocklisted or not for the user
  // on the URL of this page. Incognito Profiles will cause this to be set to
  // true. Used as a cache to avoid checking the blocklist once the page is
  // blocklisted. Once blocklisted, a page load cannot be unblocklisted.
  bool heavy_ads_blocklist_blocklisted_ = false;

  // Pointer to the blocklist used to throttle the heavy ad intervention. Can
  // be replaced by tests.
  HeavyAdBlocklist* heavy_ad_blocklist_;

  // Whether the heavy ad privacy mitigations feature is enabled.
  const bool heavy_ad_privacy_mitigations_enabled_;

  // Whether there was a heavy ad on the page at some point.
  bool heavy_ad_on_page_ = false;

  std::unique_ptr<HeavyAdThresholdNoiseProvider>
      heavy_ad_threshold_noise_provider_;

  // Whether we should only send reports, and not unload frames tagged as heavy
  // ads by the intervention. This is null until the proper feature param is
  // queried once a heavy ad is seen. Sending reports should still log entries
  // to the blocklist as it is observable by the page. Reporting only should use
  // a different message that indicates the frame was not unloaded.
  base::Optional<bool> heavy_ad_send_reports_only_;

  // Whether reports should be sent when the heavy ad intervention occurs. This
  // is null until the proper feature param is queried once a heavy ad is seen.
  base::Optional<bool> heavy_ad_reporting_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

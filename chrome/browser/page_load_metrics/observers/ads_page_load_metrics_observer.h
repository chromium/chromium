// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"

// This observer labels each sub-frame as an ad or not, and keeps track of
// relevant per-frame and whole-page byte statistics.
class AdsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver,
      public subresource_filter::SubresourceFilterObserver {
 public:
  // The types of ads that one can filter on.
  enum AdType {
    AD_TYPE_GOOGLE = 0,
    AD_TYPE_SUBRESOURCE_FILTER = 1,
    AD_TYPE_ALL = 2,
    AD_TYPE_MAX = AD_TYPE_ALL
  };

  // The origin of the ad relative to the main frame's origin.
  // Note: Logged to UMA, keep in sync with CrossOriginAdStatus in enums.xml.
  //   Add new entries to the end, and do not renumber.
  enum class AdOriginStatus {
    kUnknown = 0,
    kSame = 1,
    kCross = 2,
    kMaxValue = kCross,
  };

  // High level categories of mime types for resources loaded by the page.
  enum class ResourceMimeType {
    kJavascript = 0,
    kVideo = 1,
    kImage = 2,
    kCss = 3,
    kHtml = 4,
    kOther = 5,
    kMaxValue = kOther,
  };

  using AdTypes = std::bitset<AD_TYPE_MAX>;

  // Returns a new AdsPageLoadMetricObserver. If the feature is disabled it
  // returns nullptr.
  static std::unique_ptr<AdsPageLoadMetricsObserver> CreateIfNeeded();

  AdsPageLoadMetricsObserver();
  ~AdsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void RecordAdFrameData(FrameTreeNodeId ad_id,
                         AdTypes ad_types,
                         content::RenderFrameHost* ad_host,
                         bool frame_navigated);
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnResourceDataUseObserved(
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void OnPageInteractive(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  struct AdFrameData {
    AdFrameData(FrameTreeNodeId frame_tree_node_id,
                AdTypes ad_types,
                AdOriginStatus origin_status,
                bool frame_navigated);
    size_t frame_bytes;
    size_t frame_bytes_uncached;
    const FrameTreeNodeId frame_tree_node_id;
    AdTypes ad_types;
    AdOriginStatus origin_status;
    bool frame_navigated;
  };

  // subresource_filter::SubresourceFilterObserver:
  void OnSubframeNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      subresource_filter::LoadPolicy load_policy,
      bool is_ad_subframe) override;
  void OnAdSubframeDetected(
      content::RenderFrameHost* render_frame_host) override;
  void OnSubresourceFilterGoingAway() override;

  // Determines if the URL of a frame matches the SubresourceFilter block
  // list. Should only be called once per frame navigation.
  bool DetectSubresourceFilterAd(FrameTreeNodeId frame_tree_node_id);

  // This should only be called once per frame navigation, as the
  // SubresourceFilter detector clears its state about detected frames after
  // each call in order to free up memory.
  AdTypes DetectAds(content::NavigationHandle* navigation_handle);

  void ProcessLoadedResource(
      const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info);

  // Get the mime type of a resource. This only returns a subset of mime types,
  // grouped at a higher level. For example, all video mime types return the
  // same value.
  ResourceMimeType GetResourceMimeType(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Update all of the per-resource page counters given a new resource data
  // update. Updates |page_resources_| to reflect the new state of the resource.
  // Called once per ResourceDataUpdate.
  void UpdateResource(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Records size of resources by mime type.
  void RecordResourceMimeHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Records per-resource histograms.
  void RecordResourceHistograms(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);
  void RecordPageResourceTotalHistograms(ukm::SourceId source_id);
  void RecordHistograms(ukm::SourceId source_id);
  void RecordHistogramsForType(int ad_type);

  // Checks to see if a resource is waiting for a navigation with the given
  // |frame_tree_node_id| to commit before it can be processed. If so, call
  // OnLoadedResource for the delayed resource.
  void ProcessOngoingNavigationResource(FrameTreeNodeId frame_tree_node_id);

  // Stores the size data of each ad frame. Pointed to by ad_frames_ so use a
  // data structure that won't move the data around.
  std::list<AdFrameData> ad_frames_data_storage_;

  // Maps a frame (by id) to the AdFrameData responsible for the frame.
  // Multiple frame ids can point to the same AdFrameData. The responsible
  // frame is the top-most frame labeled as an ad in the frame's ancestry,
  // which may be itself. If no responsible frame is found, the data is
  // nullptr.
  std::map<FrameTreeNodeId, AdFrameData*> ad_frames_data_;

  // The set of frames that have yet to finish but that the SubresourceFilter
  // has reported are ads. Once DetectSubresourceFilterAd is called the id is
  // removed from the set.
  std::set<FrameTreeNodeId> unfinished_subresource_ad_frames_;

  // When the observer receives report of a document resource loading for a
  // sub-frame before the sub-frame commit occurs, hold onto the resource
  // request info (delay it) until the sub-frame commits.
  std::map<FrameTreeNodeId, page_load_metrics::ExtraRequestCompleteInfo>
      ongoing_navigation_resources_;

  // Maps a request_id for a blink resource to the metadata for the resource
  // load. Only contains resources that have not completed loading.
  std::map<int, page_load_metrics::mojom::ResourceDataUpdatePtr>
      page_resources_;

  // Tallies for bytes and counts observed in resource data updates for the
  // entire page.
  size_t page_ad_javascript_bytes_ = 0u;
  size_t page_ad_video_bytes_ = 0u;
  size_t page_resource_bytes_ = 0u;
  size_t page_ad_resource_bytes_ = 0u;
  size_t page_main_frame_ad_resource_bytes_ = 0u;
  uint32_t total_number_page_resources_ = 0;

  // Time the page was committed.
  base::Time time_commit_;

  // Time the page was observed to be interactive.
  base::Time time_interactive_;

  // Total ad bytes loaded by the page since it was observed to be interactive.
  size_t page_ad_resource_bytes_since_interactive_ = 0u;

  size_t page_bytes_ = 0u;
  size_t uncached_page_bytes_ = 0u;
  bool committed_ = false;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      subresource_observer_;

  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_

// TODO(crbug.com/1136068): Split this file up so that its various classes and
// enums are in different files as well.

#include <stdint.h>

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

// Resource usage thresholds for the Heavy Ad Intervention feature. These
// numbers are platform specific and are intended to target 1 in 1000 ad iframes
// on each platform, for network and CPU use respectively.
namespace heavy_ad_thresholds {

// Maximum number of network bytes allowed to be loaded by a frame. These
// numbers reflect the 99.9th percentile of the
// PageLoad.Clients.Ads.Bytes.AdFrames.PerFrame.Network histogram on mobile and
// desktop. Additive noise is added to this threshold, see
// AdsPageLoadMetricsObserver::HeavyAdThresholdNoiseProvider.
const int kMaxNetworkBytes = 4.0 * 1024 * 1024;

// CPU thresholds are selected from AdFrameLoad UKM, and are intended to target
// 1 in 1000 ad iframes combined, with each threshold responsible for roughly
// half of those intervention. Maximum number of milliseconds of CPU use allowed
// to be used by a frame.
const int kMaxCpuTime = 60 * 1000;

// Maximum percentage of CPU utilization over a 30 second window allowed.
const int kMaxPeakWindowedPercent = 50;

}  // namespace heavy_ad_thresholds

namespace ad_metrics {

// The origin of the ad relative to the main frame's origin.
// Note: Logged to UMA, keep in sync with CrossOriginAdStatus in enums.xml.
//   Add new entries to the end, and do not renumber.
enum class OriginStatus {
  kUnknown = 0,
  kSame = 1,
  kCross = 2,
  kMaxValue = kCross,
};

// Origin status further broken down by whether the ad frame tree has a
// frame currently not render-throttled (i.e. is eligible to be painted).
// Note that since creative origin status is based on first contentful paint,
// only ad frame trees with unknown creative origin status can be without any
// frames that are eligible to be painted.
// Note: Logged to UMA, keep in sync with
// CrossOriginCreativeStatusWithThrottling in enums.xml.
// Add new entries to the end, and do not renumber.
enum class OriginStatusWithThrottling {
  kUnknownAndUnthrottled = 0,
  kUnknownAndThrottled = 1,
  kSameAndUnthrottled = 2,
  kCrossAndUnthrottled = 3,
  kMaxValue = kCrossAndUnthrottled,
};

// The type of heavy ad this frame is classified as per the Heavy Ad
// Intervention.
enum class HeavyAdStatus {
  kNone = 0,
  kNetwork = 1,
  kTotalCpu = 2,
  kPeakCpu = 3,
  kMaxValue = kPeakCpu,
};

// Controls what values of HeavyAdStatus will be cause an unload due to the
// intervention.
enum class HeavyAdUnloadPolicy {
  kNetworkOnly = 0,
  kCpuOnly = 1,
  kAll = 2,
};

// Represents how a frame should be treated by the heavy ad intervention.
enum class HeavyAdAction {
  // Nothing should be done, i.e. the ad is not heavy or the intervention is
  // not enabled.
  kNone = 0,
  // The ad should be reported as heavy.
  kReport = 1,
  // The ad should be reported and unloaded.
  kUnload = 2,
  // The frame was ignored, i.e. the blocklist was full or page is a reload.
  kIgnored = 3,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. For any additions, also update the
// corresponding PageEndReason enum in enums.xml.
enum class UserActivationStatus {
  kNoActivation = 0,
  kReceivedActivation = 1,
  kMaxValue = kReceivedActivation,
};

// TODO(crbug.com/1136068): The above enums should sit in FrameTreeData.

// Whether or not the ad frame has a display: none styling.
enum FrameVisibility {
  kNonVisible = 0,
  kVisible = 1,
  kAnyVisibility = 2,
  kMaxValue = kAnyVisibility,
};

// High level categories of mime types for resources loaded by the frame.
enum class ResourceMimeType {
  kJavascript = 0,
  kVideo = 1,
  kImage = 2,
  kCss = 3,
  kHtml = 4,
  kOther = 5,
  kMaxValue = kOther,
};

// Whether or not media has been played in this frame. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MediaStatus {
  kNotPlayed = 0,
  kPlayed = 1,
  kMaxValue = kPlayed,
};

class ResourceLoadAggregator {
 public:
  ResourceLoadAggregator();
  ~ResourceLoadAggregator();

  // Updates the number of bytes loaded in the frame given a resource load.
  void ProcessResourceLoad(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Adds additional bytes to the ad resource byte counts. This
  // is used to notify the frame that some bytes were tagged as ad bytes after
  // they were loaded.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes, ResourceMimeType mime_type);

  // Get the mime type of a resource. This only returns a subset of mime types,
  // grouped at a higher level. For example, all video mime types return the
  // same value.
  // TODO(crbug.com/1136068): This is used well out of the scope of the
  // AdsPageLoadMetricsObserver and should sit in a common directory.
  static ResourceMimeType GetResourceMimeType(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource);

  // Accessors for the various data stored in the class.

  size_t bytes() const { return bytes_; }

  size_t network_bytes() const { return network_bytes_; }

  size_t ad_bytes() const { return ad_bytes_; }

  size_t ad_network_bytes() const { return ad_network_bytes_; }

  size_t GetAdNetworkBytesForMime(ResourceMimeType mime_type) const {
    return ad_bytes_by_mime_[static_cast<size_t>(mime_type)];
  }

 private:
  // Total bytes used to load resources in the frame, including headers.
  size_t bytes_ = 0u;
  size_t network_bytes_ = 0u;

  // Ad network bytes for different mime type resources loaded in the frame.
  size_t ad_bytes_by_mime_[static_cast<size_t>(ResourceMimeType::kMaxValue) +
                           1] = {0};

  // Tracks the number of bytes that were used to load resources which were
  // detected to be ads inside of this frame. For ad frames, these counts should
  // match |frame_bytes| and |frame_network_bytes|.
  size_t ad_bytes_ = 0u;
  size_t ad_network_bytes_ = 0u;
};

class PeakCpuAggregator {
 public:
  // Standard constructor / desturctor.
  PeakCpuAggregator();
  ~PeakCpuAggregator();

  // Window over which to consider cpu time spent in an ad_frame.
  static constexpr base::TimeDelta kWindowSize =
      base::TimeDelta::FromSeconds(30);

  // Update the peak window variables given the current update and update time.
  void UpdatePeakWindowedPercent(base::TimeDelta cpu_usage_update,
                                 base::TimeTicks update_time);

  // Accessor for the peak percent.
  int peak_windowed_percent() const { return peak_windowed_percent_; }

 private:
  // Time updates for the frame with a timestamp indicating when they arrived.
  // Used for windowed cpu load reporting.
  struct CpuUpdateData {
    base::TimeTicks update_time;
    base::TimeDelta usage_info;
    CpuUpdateData(base::TimeTicks time, base::TimeDelta info)
        : update_time(time), usage_info(info) {}
  };

  // The cpu time spent in the current window.
  base::TimeDelta current_window_total_;

  // The cpu updates themselves that are still relevant for the time window.
  // Note: Since the window is 30 seconds and PageLoadMetrics updates arrive at
  // most every half second, this can never have more than 60 elements.
  base::queue<CpuUpdateData> current_window_updates_;

  // The peak windowed cpu load during the unactivated period.
  int peak_windowed_percent_ = 0;
};

class MemoryUsageAggregator {
 public:
  void UpdateUsage(int64_t delta_bytes);

  uint64_t max_bytes_used() const { return max_bytes_used_; }

 private:
  // Maximum concurrent memory usage by V8 in this ad frame tree.
  // Tracks max value of |v8_current_memory_bytes_used_| for this frame tree.
  uint64_t max_bytes_used_ = 0UL;

  // Current concurrent memory usage by V8 in this ad frame tree.
  // Computation is best-effort, as it relies on individual asynchronous
  // per-frame measurements, some of which may be stale.
  uint64_t current_bytes_used_ = 0UL;
};

// FrameTreeData represents a frame along with its entire subtree, and is
// typically used to capture an ad creative. It stores frame-specific
// information (such as size, activation status, and origin), which is typically
// specific to the top frame in the tree.
class FrameTreeData : public base::SupportsWeakPtr<FrameTreeData> {
 public:
  using FrameTreeNodeId =
      page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;

  // |root_frame_tree_node_id| is the root frame of the subtree that
  // FrameTreeData stores information for.
  explicit FrameTreeData(FrameTreeNodeId root_frame_tree_node_id,
                         int heavy_ad_network_threshold_noise);
  ~FrameTreeData();

  // Processes a resource load in frame, calling ResourceLoadAggregator.
  void ProcessResourceLoadInFrame(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource,
      int process_id,
      const page_load_metrics::ResourceTracker& resource_tracker);

  // Adjusts ad bytes after call to ProcessResourceLoadInFrame, calling
  // ResourceLoadAggregator.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes, ResourceMimeType mime_type);

  // Updates the cpu usage of this frame.
  void UpdateCpuUsage(base::TimeTicks update_time, base::TimeDelta update);

  // Get the total cpu usage of this frame;
  base::TimeDelta GetTotalCpuUsage() const;

  // Update the metadata of this frame if it is being navigated.
  void UpdateForNavigation(content::RenderFrameHost* render_frame_host,
                           bool frame_navigated);

  // Returns how the frame should be treated by the heavy ad intervention.
  // This intervention is triggered when the frame is considered heavy, has not
  // received user gesture, and the intervention feature is enabled. This
  // returns an action the first time the criteria is met, and false afterwards.
  HeavyAdAction MaybeTriggerHeavyAdIntervention();

  // Updates the max frame depth of this frames tree given the newly seen child
  // frame.
  void MaybeUpdateFrameDepth(content::RenderFrameHost* render_frame_host);

  // Updates the recorded bytes of memory used.
  void UpdateMemoryUsage(int64_t delta_bytes);

  // Returns whether the frame should be recorded for UKMs and UMA histograms.
  // A frame should be recorded if it has non-zero bytes or non-zero CPU usage
  // (or both).
  bool ShouldRecordFrameForMetrics() const;

  // Construct and record an AdFrameLoad UKM event for this frame. Only records
  // events for frames that have non-zero bytes.
  void RecordAdFrameLoadUkmEvent(ukm::SourceId source_id) const;

  // Returns the corresponding enum value to split the creative origin status
  // by whether any frame in the ad frame tree is throttled.
  OriginStatusWithThrottling GetCreativeOriginStatusWithThrottling() const;

  // Get the cpu usage for the appropriate activation period.
  base::TimeDelta GetActivationCpuUsage(UserActivationStatus status) const {
    return cpu_usage_[static_cast<size_t>(status)];
  }

  FrameTreeNodeId root_frame_tree_node_id() const {
    return root_frame_tree_node_id_;
  }

  OriginStatus origin_status() const { return origin_status_; }

  OriginStatus creative_origin_status() const {
    return creative_origin_status_;
  }

  base::Optional<base::TimeDelta> first_eligible_to_paint() const {
    return first_eligible_to_paint_;
  }

  base::Optional<base::TimeDelta> earliest_first_contentful_paint() const {
    return earliest_first_contentful_paint_;
  }
  // Sets the size of the frame and updates its visibility state.
  void SetFrameSize(gfx::Size frame_size_);

  // Sets the display state of the frame and updates its visibility state.
  void SetDisplayState(bool is_display_none);

  uint64_t v8_max_memory_bytes_used() const {
    return memory_usage_.max_bytes_used();
  }

  UserActivationStatus user_activation_status() const {
    return user_activation_status_;
  }

  bool frame_navigated() const { return frame_navigated_; }

  FrameVisibility visibility() const { return visibility_; }

  gfx::Size frame_size() const { return frame_size_; }

  bool is_display_none() const { return is_display_none_; }

  MediaStatus media_status() const { return media_status_; }

  void set_media_status(MediaStatus media_status) {
    media_status_ = media_status;
  }

  // Records that the sticky user activation bit has been set on the frame.
  // Cannot be unset.
  void set_received_user_activation() {
    user_activation_status_ = UserActivationStatus::kReceivedActivation;
  }

  void set_creative_origin_status(OriginStatus creative_origin_status) {
    creative_origin_status_ = creative_origin_status;
  }

  void SetFirstEligibleToPaint(base::Optional<base::TimeDelta> time_stamp);

  // Returns whether a new FCP is set.
  bool SetEarliestFirstContentfulPaint(
      base::Optional<base::TimeDelta> time_stamp);

  HeavyAdStatus heavy_ad_status() const { return heavy_ad_status_; }

  HeavyAdStatus heavy_ad_status_with_noise() const {
    return heavy_ad_status_with_noise_;
  }

  HeavyAdStatus heavy_ad_status_with_policy() const {
    return heavy_ad_status_with_policy_;
  }

  void set_heavy_ad_action(HeavyAdAction heavy_ad_action) {
    heavy_ad_action_ = heavy_ad_action;
  }

  // Accessor for the total resource data of the frame tree.
  const ResourceLoadAggregator& resource_data() const { return resource_data_; }

  // Accessor for the peak windowed cpu usage of the frame tree.
  int peak_windowed_cpu_percent() const {
    return peak_cpu_.peak_windowed_percent();
  }

 private:
  // Updates whether or not this frame meets the criteria for visibility.
  void UpdateFrameVisibility();

  // Computes whether this frame meets the criteria for being a heavy frame for
  // the heavy ad intervention and returns the type of threshold hit if any.
  // If |use_network_threshold_noise| is set,
  // |heavy_ad_network_threshold_noise_| is added to the network threshold when
  // computing the status. |policy| controls which thresholds are used when
  // computing the status.
  HeavyAdStatus ComputeHeavyAdStatus(bool use_network_threshold_noise,
                                     HeavyAdUnloadPolicy policy) const;

  // The frame tree node id of root frame of the subtree that |this| is
  // tracking information for.
  const FrameTreeNodeId root_frame_tree_node_id_;

  // TODO(ericrobinson): May want to move this to ResourceLoadAggregator.
  // Number of resources loaded by the frame (both complete and incomplete).
  int num_resources_ = 0;

  // The depth of this FrameTreeData's root frame.
  unsigned int root_frame_depth_ = 0;

  // The max depth of this frames frame tree.
  unsigned int frame_depth_ = 0;

  // The origin status of the ad frame for the creative.
  OriginStatus origin_status_ = OriginStatus::kUnknown;

  // The origin status of the creative content.
  OriginStatus creative_origin_status_ = OriginStatus::kUnknown;

  // Whether or not the frame has been navigated, to determine if there's an
  // ongoing request.
  bool frame_navigated_ = false;

  // Whether or not the frame is set to not display.
  bool is_display_none_ = false;

  // Whether or not a creative is large enough to be visible by the user.
  FrameVisibility visibility_ = FrameVisibility::kVisible;

  // The size of the frame.
  gfx::Size frame_size_;

  // Whether or not the frame has started media playing.
  MediaStatus media_status_ = MediaStatus::kNotPlayed;

  // Earliest time that any frame in the ad frame tree has reported
  // as being eligible to paint, or null if all frames are currently
  // render-throttled and there hasn't been a first paint. Note that this
  // timestamp and the implied throttling status are best-effort.
  base::Optional<base::TimeDelta> first_eligible_to_paint_;

  // The smallest FCP seen for any any frame in this ad frame tree, if a
  // frame has painted.
  base::Optional<base::TimeDelta> earliest_first_contentful_paint_;

  // Indicates whether or not this frame met the criteria for the heavy ad
  // intervention.
  HeavyAdStatus heavy_ad_status_ = HeavyAdStatus::kNone;

  // Same as |heavy_ad_status_| but uses additional additive noise for the
  // network threshold. A frame can be considered a heavy ad by
  // |heavy_ad_status_| but not |heavy_ad_status_with_noise_|. The noised
  // threshold is used when determining whether to actually trigger the
  // intervention.
  HeavyAdStatus heavy_ad_status_with_noise_ = HeavyAdStatus::kNone;

  // Same as |heavy_ad_status_with_noise_| but selectively uses thresholds based
  // on a field trial param. This status is used to control when the
  // intervention fires.
  HeavyAdStatus heavy_ad_status_with_policy_ = HeavyAdStatus::kNone;

  // The action taken on this frame by the heavy ad intervention if any.
  HeavyAdAction heavy_ad_action_ = HeavyAdAction::kNone;

  // Number of bytes of noise that should be added to the network threshold.
  const int heavy_ad_network_threshold_noise_;

  // Whether or not the frame has been activated (clicked on).
  UserActivationStatus user_activation_status_ =
      UserActivationStatus::kNoActivation;

  // The cpu usage for both the activated and unactivated time periods.
  base::TimeDelta
      cpu_usage_[static_cast<size_t>(UserActivationStatus::kMaxValue) + 1];

  // The resource data for this frame tree.
  ResourceLoadAggregator resource_data_;

  // The peak cpu usage for this frame tree.
  PeakCpuAggregator peak_cpu_;

  // Memory usage by v8 in this ad frame tree.
  MemoryUsageAggregator memory_usage_;
};

// AggregateFrameData stores information in aggregate for all the frames during
// a navigation.  It contains specific information on various types of frames
// and their usage, such as ad vs. non-ad frames, as well as information about
// usage across the navigation as a whole.
class AggregateFrameData {
 public:
  AggregateFrameData();
  ~AggregateFrameData();

  void ProcessResourceLoadInFrame(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource,
      bool is_main_frame);

  // Adjusts the overall page and potentially main frame ad bytes.
  void AdjustAdBytes(int64_t unaccounted_ad_bytes,
                     ResourceMimeType mime_type,
                     bool is_main_frame);

  // Updates the cpu usage for the page, given whether update is for an ad.
  void UpdateCpuUsage(base::TimeTicks update_time,
                      base::TimeDelta update,
                      bool is_ad);

  int peak_windowed_non_ad_cpu_percent() const {
    return non_ad_peak_cpu_.peak_windowed_percent();
  }

  int peak_windowed_cpu_percent() const {
    return total_peak_cpu_.peak_windowed_percent();
  }

  // TODO(crbug.com/1136068): The size_t members should probably be int64_t.
  struct AdDataByVisibility {
    // The following are aggregated when metrics are recorded on navigation.
    size_t bytes = 0;
    size_t network_bytes = 0;
    size_t frames = 0;
    // MemoryUsage is aggregated when a memory update is received.
    MemoryUsageAggregator memory;
  };

  // Returns the appropriate AdDataByVisibility given the |visibility|.
  const AdDataByVisibility& get_ad_data_by_visibility(
      FrameVisibility visibility) {
    return ad_data_[static_cast<size_t>(visibility)];
  }

  // These functions update the various members of AdDataByVisibility given the
  // visibility.  They all increment the current value.
  void update_ad_bytes_by_visibility(FrameVisibility visibility, size_t bytes) {
    ad_data_[static_cast<size_t>(visibility)].bytes += bytes;
  }
  void update_ad_network_bytes_by_visibility(FrameVisibility visibility,
                                             size_t network_bytes) {
    ad_data_[static_cast<size_t>(visibility)].network_bytes += network_bytes;
  }
  void update_ad_frames_by_visibility(FrameVisibility visibility,
                                      size_t frames) {
    ad_data_[static_cast<size_t>(visibility)].frames += frames;
  }
  void update_ad_memory_by_visibility(FrameVisibility visibility,
                                      int64_t delta_bytes) {
    ad_data_[static_cast<size_t>(visibility)].memory.UpdateUsage(delta_bytes);
  }

  // Updates the memory for the main frame of the page.
  void update_main_frame_memory(int64_t delta_memory) {
    main_frame_memory_.UpdateUsage(delta_memory);
  }

  // Updates the total ad cpu usage for the page.
  void update_ad_cpu_usage(base::TimeDelta usage) { ad_cpu_usage_ += usage; }

  // Get the total memory usage for this page.
  int64_t main_frame_max_memory() const {
    return main_frame_memory_.max_bytes_used();
  }

  // Get the total cpu usage of this page.
  base::TimeDelta total_cpu_usage() const { return cpu_usage_; }
  base::TimeDelta total_ad_cpu_usage() const { return ad_cpu_usage_; }

  // Accessor for the total resource data of the page.
  const ResourceLoadAggregator& resource_data() const { return resource_data_; }
  const ResourceLoadAggregator& main_frame_resource_data() const {
    return main_frame_resource_data_;
  }

 private:
  // Stores the data for ads on a page according to visibility.
  AdDataByVisibility
      ad_data_[static_cast<size_t>(FrameVisibility::kMaxValue) + 1] = {};

  // The overall cpu usage for this page.
  base::TimeDelta cpu_usage_ = base::TimeDelta();
  base::TimeDelta ad_cpu_usage_ = base::TimeDelta();

  // The memory used by the main frame.
  MemoryUsageAggregator main_frame_memory_;

  // The resource data for this page.
  ResourceLoadAggregator resource_data_;
  ResourceLoadAggregator main_frame_resource_data_;

  // The peak cpu usages for this page.
  PeakCpuAggregator total_peak_cpu_;
  PeakCpuAggregator non_ad_peak_cpu_;
};

}  // namespace ad_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AD_METRICS_FRAME_DATA_H_

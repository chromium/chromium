// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace {

using OriginStatus = FrameData::OriginStatus;
using OriginStatusWithThrottling = FrameData::OriginStatusWithThrottling;

// A frame with area less than kMinimumVisibleFrameArea is not considered
// visible.
const int kMinimumVisibleFrameArea = 25;

// Controls what types of heavy ads will be unloaded by the intervention.
const base::FeatureParam<int> kHeavyAdUnloadPolicyParam = {
    &features::kHeavyAdIntervention, "kUnloadPolicy",
    static_cast<int>(FrameData::HeavyAdUnloadPolicy::kAll)};

}  // namespace

// static
constexpr base::TimeDelta FrameData::kCpuWindowSize;

// static
FrameData::ResourceMimeType FrameData::GetResourceMimeType(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  if (blink::IsSupportedImageMimeType(resource->mime_type))
    return ResourceMimeType::kImage;
  if (blink::IsSupportedJavascriptMimeType(resource->mime_type))
    return ResourceMimeType::kJavascript;

  std::string top_level_type;
  std::string subtype;
  // Categorize invalid mime types as "Other".
  if (!net::ParseMimeTypeWithoutParameter(resource->mime_type, &top_level_type,
                                          &subtype)) {
    return ResourceMimeType::kOther;
  }
  if (top_level_type.compare("video") == 0)
    return ResourceMimeType::kVideo;
  if (top_level_type.compare("text") == 0 && subtype.compare("css") == 0)
    return ResourceMimeType::kCss;
  if (top_level_type.compare("text") == 0 && subtype.compare("html") == 0)
    return ResourceMimeType::kHtml;
  return ResourceMimeType::kOther;
}

FrameData::FrameData(FrameTreeNodeId root_frame_tree_node_id,
                     int heavy_ad_network_threshold_noise)
    : root_frame_tree_node_id_(root_frame_tree_node_id),
      bytes_(0u),
      network_bytes_(0u),
      same_origin_bytes_(0u),
      origin_status_(OriginStatus::kUnknown),
      creative_origin_status_(OriginStatus::kUnknown),
      frame_navigated_(false),
      user_activation_status_(UserActivationStatus::kNoActivation),
      is_display_none_(false),
      visibility_(FrameVisibility::kVisible),
      frame_size_(gfx::Size()),
      heavy_ad_status_(HeavyAdStatus::kNone),
      heavy_ad_status_with_noise_(HeavyAdStatus::kNone),
      heavy_ad_network_threshold_noise_(heavy_ad_network_threshold_noise) {}

FrameData::~FrameData() = default;

void FrameData::UpdateForNavigation(content::RenderFrameHost* render_frame_host,
                                    bool frame_navigated) {
  frame_navigated_ = frame_navigated;
  if (!render_frame_host)
    return;

  SetDisplayState(render_frame_host->IsFrameDisplayNone());
  if (render_frame_host->GetFrameSize())
    SetFrameSize(*(render_frame_host->GetFrameSize()));

  // For frames triggered on render, their origin is their parent's origin.
  origin_status_ =
      AdsPageLoadMetricsObserver::IsSubframeSameOriginToMainFrame(
          render_frame_host, !frame_navigated /* use_parent_origin */)
          ? OriginStatus::kSame
          : OriginStatus::kCross;

  origin_ = frame_navigated
                ? render_frame_host->GetLastCommittedOrigin()
                : render_frame_host->GetParent()->GetLastCommittedOrigin();

  root_frame_depth_ = render_frame_host->GetFrameDepth();
}

void FrameData::ProcessResourceLoadInFrame(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource,
    int process_id,
    const page_load_metrics::ResourceTracker& resource_tracker) {
  bool is_same_origin = origin_.IsSameOriginWith(resource->origin);
  bytes_ += resource->delta_bytes;
  network_bytes_ += resource->delta_bytes;
  if (is_same_origin)
    same_origin_bytes_ += resource->delta_bytes;

  content::GlobalRequestID global_id(process_id, resource->request_id);
  if (!resource_tracker.HasPreviousUpdateForResource(global_id))
    num_resources_++;

  // Report cached resource body bytes to overall frame bytes.
  if (resource->is_complete &&
      resource->cache_type != page_load_metrics::mojom::CacheType::kNotCached) {
    bytes_ += resource->encoded_body_length;
    if (is_same_origin)
      same_origin_bytes_ += resource->encoded_body_length;
  }

  if (resource->reported_as_ad_resource) {
    ad_network_bytes_ += resource->delta_bytes;
    ad_bytes_ += resource->delta_bytes;
    // Report cached resource body bytes to overall frame bytes.
    if (resource->is_complete &&
        resource->cache_type != page_load_metrics::mojom::CacheType::kNotCached)
      ad_bytes_ += resource->encoded_body_length;

    ResourceMimeType mime_type = GetResourceMimeType(resource);
    ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += resource->delta_bytes;
  }
}

void FrameData::AdjustAdBytes(int64_t unaccounted_ad_bytes,
                              ResourceMimeType mime_type) {
  ad_network_bytes_ += unaccounted_ad_bytes;
  ad_bytes_ += unaccounted_ad_bytes;
  ad_bytes_by_mime_[static_cast<size_t>(mime_type)] += unaccounted_ad_bytes;
}

void FrameData::SetFrameSize(gfx::Size frame_size) {
  frame_size_ = frame_size;
  UpdateFrameVisibility();
}

void FrameData::SetDisplayState(bool is_display_none) {
  is_display_none_ = is_display_none;
  UpdateFrameVisibility();
}

void FrameData::UpdateCpuUsage(base::TimeTicks update_time,
                               base::TimeDelta update) {
  // Update the overall usage for all of the relevant buckets.
  cpu_by_activation_period_[static_cast<size_t>(user_activation_status_)] +=
      update;

  // If the frame has been activated, then we don't update the peak usage.
  if (user_activation_status_ == UserActivationStatus::kReceivedActivation)
    return;

  // Update the peak usage.
  cpu_total_for_current_window_ += update;
  cpu_updates_for_current_window_.push(CpuUpdateData(update_time, update));
  base::TimeTicks cutoff_time = update_time - kCpuWindowSize;
  while (!cpu_updates_for_current_window_.empty() &&
         cpu_updates_for_current_window_.front().update_time < cutoff_time) {
    cpu_total_for_current_window_ -=
        cpu_updates_for_current_window_.front().usage_info;
    cpu_updates_for_current_window_.pop();
  }
  int current_windowed_cpu_percent =
      100 * cpu_total_for_current_window_.InMilliseconds() /
      kCpuWindowSize.InMilliseconds();
  if (current_windowed_cpu_percent > peak_windowed_cpu_percent_) {
    peak_windowed_cpu_percent_ = current_windowed_cpu_percent;
    peak_window_start_time_ =
        cpu_updates_for_current_window_.front().update_time;
  }
}

FrameData::HeavyAdAction FrameData::MaybeTriggerHeavyAdIntervention() {
  // TODO(johnidel): This method currently does a lot of heavy lifting: tracking
  // noised and unnoised metrics, determining feature action, and branching
  // based on configuration. Consider splitting this out and letting AdsPLMO do
  // more of the feature specific logic.
  //
  // If the intervention has already performed an action on this frame, do not
  // perform another. Metrics will have been calculated already.
  if (user_activation_status_ == UserActivationStatus::kReceivedActivation ||
      heavy_ad_action_ != HeavyAdAction::kNone) {
    return HeavyAdAction::kNone;
  }

  // Update heavy ad related metrics. Metrics are reported for all thresholds,
  // regardless of unload policy.
  if (heavy_ad_status_ == HeavyAdStatus::kNone) {
    heavy_ad_status_ = ComputeHeavyAdStatus(
        false /* use_network_threshold_noise */, HeavyAdUnloadPolicy::kAll);
  }
  if (heavy_ad_status_with_noise_ == HeavyAdStatus::kNone) {
    heavy_ad_status_with_noise_ = ComputeHeavyAdStatus(
        true /* use_network_threshold_noise */, HeavyAdUnloadPolicy::kAll);
  }

  // Only activate the field trial if there is a heavy ad. Getting the feature
  // param value activates the trial, so we cannot limit activating the trial
  // based on the HeavyAdUnloadPolicy. Therefore, we just use a heavy ad of any
  // type as a gate for activating trial.
  if (heavy_ad_status_with_noise_ == HeavyAdStatus::kNone)
    return HeavyAdAction::kNone;

  heavy_ad_status_with_policy_ = ComputeHeavyAdStatus(
      true /* use_network_threshold_noise */,
      static_cast<HeavyAdUnloadPolicy>(kHeavyAdUnloadPolicyParam.Get()));

  if (heavy_ad_status_with_policy_ == HeavyAdStatus::kNone)
    return HeavyAdAction::kNone;

  // Only check if the feature is enabled once we have a heavy ad. This is done
  // to ensure that any experiment for this feature will only be comparing
  // groups who have seen a heavy ad.
  if (!base::FeatureList::IsEnabled(features::kHeavyAdIntervention)) {
    // If the intervention is not enabled, we return whether reporting is
    // enabled.
    return base::FeatureList::IsEnabled(features::kHeavyAdInterventionWarning)
               ? HeavyAdAction::kReport
               : HeavyAdAction::kNone;
  }

  return HeavyAdAction::kUnload;
}

base::TimeDelta FrameData::GetActivationCpuUsage(
    UserActivationStatus status) const {
  return cpu_by_activation_period_[static_cast<int>(status)];
}

base::TimeDelta FrameData::GetTotalCpuUsage() const {
  base::TimeDelta total_cpu_time;
  for (base::TimeDelta cpu_time : cpu_by_activation_period_)
    total_cpu_time += cpu_time;
  return total_cpu_time;
}

size_t FrameData::GetAdNetworkBytesForMime(ResourceMimeType mime_type) const {
  return ad_bytes_by_mime_[static_cast<size_t>(mime_type)];
}

void FrameData::MaybeUpdateFrameDepth(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;
  DCHECK_GE(render_frame_host->GetFrameDepth(), root_frame_depth_);
  if (render_frame_host->GetFrameDepth() - root_frame_depth_ > frame_depth_)
    frame_depth_ = render_frame_host->GetFrameDepth() - root_frame_depth_;
}

bool FrameData::ShouldRecordFrameForMetrics() const {
  return bytes() != 0 || !GetTotalCpuUsage().is_zero();
}

void FrameData::RecordAdFrameLoadUkmEvent(ukm::SourceId source_id) const {
  if (!ShouldRecordFrameForMetrics())
    return;

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::AdFrameLoad builder(source_id);
  builder
      .SetLoading_NetworkBytes(
          ukm::GetExponentialBucketMinForBytes(network_bytes()))
      .SetLoading_CacheBytes2(
          ukm::GetExponentialBucketMinForBytes((bytes() - network_bytes())))
      .SetLoading_VideoBytes(ukm::GetExponentialBucketMinForBytes(
          GetAdNetworkBytesForMime(ResourceMimeType::kVideo)))
      .SetLoading_JavascriptBytes(ukm::GetExponentialBucketMinForBytes(
          GetAdNetworkBytesForMime(ResourceMimeType::kJavascript)))
      .SetLoading_ImageBytes(ukm::GetExponentialBucketMinForBytes(
          GetAdNetworkBytesForMime(ResourceMimeType::kImage)))
      .SetLoading_NumResources(num_resources_);

  builder.SetCpuTime_Total(GetTotalCpuUsage().InMilliseconds());
  if (user_activation_status() == UserActivationStatus::kReceivedActivation) {
    builder.SetCpuTime_PreActivation(
        GetActivationCpuUsage(UserActivationStatus::kNoActivation)
            .InMilliseconds());
  }

  builder.SetCpuTime_PeakWindowedPercent(peak_windowed_cpu_percent_);

  builder
      .SetVisibility_FrameWidth(
          ukm::GetExponentialBucketMinForCounts1000(frame_size().width()))
      .SetVisibility_FrameHeight(
          ukm::GetExponentialBucketMinForCounts1000(frame_size().height()))
      .SetVisibility_Hidden(is_display_none_);

  builder.SetStatus_CrossOrigin(static_cast<int>(origin_status()))
      .SetStatus_Media(static_cast<int>(media_status()))
      .SetStatus_UserActivation(static_cast<int>(user_activation_status()));

  builder.SetFrameDepth(frame_depth_);

  if (auto earliest_fcp = earliest_first_contentful_paint()) {
    builder.SetTiming_FirstContentfulPaint(earliest_fcp->InMilliseconds());
  }
  builder.Record(ukm_recorder->Get());
}

FrameData::OriginStatusWithThrottling
FrameData::GetCreativeOriginStatusWithThrottling() const {
  bool is_throttled = !first_eligible_to_paint().has_value();

  switch (creative_origin_status()) {
    case OriginStatus::kUnknown:
      return is_throttled ? OriginStatusWithThrottling::kUnknownAndThrottled
                          : OriginStatusWithThrottling::kUnknownAndUnthrottled;
    case OriginStatus::kSame:
      DCHECK(!is_throttled);
      return OriginStatusWithThrottling::kSameAndUnthrottled;
    case OriginStatus::kCross:
      DCHECK(!is_throttled);
      return OriginStatusWithThrottling::kCrossAndUnthrottled;
    // We expect the above values to cover all cases.
    default:
      NOTREACHED();
      return OriginStatusWithThrottling::kUnknownAndUnthrottled;
  }
}

void FrameData::SetFirstEligibleToPaint(
    base::Optional<base::TimeDelta> time_stamp) {
  if (time_stamp.has_value()) {
    // If the ad frame tree hasn't already received an earlier paint
    // eligibility stamp, mark it as eligible to paint. Since multiple frames
    // may report timestamps, we keep the earliest reported stamp.
    // Note that this timestamp (or lack thereof) is best-effort.
    if (!first_eligible_to_paint_.has_value() ||
        first_eligible_to_paint_.value() > time_stamp.value())
      first_eligible_to_paint_ = time_stamp;
  } else if (!earliest_first_contentful_paint_.has_value()) {
    // If a frame in this ad frame tree has already painted, there is no
    // further need to update paint eligibility. But if nothing has
    // painted and a null value is passed into the setter, that means the
    // frame is now render-throttled and we should reset the paint-eligiblity
    // value.
    first_eligible_to_paint_.reset();
  }
}

bool FrameData::SetEarliestFirstContentfulPaint(
    base::Optional<base::TimeDelta> time_stamp) {
  if (!time_stamp.has_value() || time_stamp.value().is_zero())
    return false;

  if (earliest_first_contentful_paint_.has_value() &&
      time_stamp.value() >= earliest_first_contentful_paint_.value())
    return false;

  earliest_first_contentful_paint_ = time_stamp;
  return true;
}

void FrameData::UpdateFrameVisibility() {
  visibility_ =
      !is_display_none_ &&
              frame_size_.GetCheckedArea().ValueOrDefault(
                  std::numeric_limits<int>::max()) >= kMinimumVisibleFrameArea
          ? FrameVisibility::kVisible
          : FrameVisibility::kNonVisible;
}

FrameData::HeavyAdStatus FrameData::ComputeHeavyAdStatus(
    bool use_network_threshold_noise,
    HeavyAdUnloadPolicy policy) const {
  if (policy == HeavyAdUnloadPolicy::kCpuOnly ||
      policy == HeavyAdUnloadPolicy::kAll) {
    // Check if the frame meets the peak CPU usage threshold.
    if (peak_windowed_cpu_percent_ >=
        heavy_ad_thresholds::kMaxPeakWindowedPercent) {
      return HeavyAdStatus::kPeakCpu;
    }

    // Check if the frame meets the absolute CPU time threshold.
    if (GetTotalCpuUsage().InMilliseconds() >= heavy_ad_thresholds::kMaxCpuTime)
      return HeavyAdStatus::kTotalCpu;
  }

  if (policy == HeavyAdUnloadPolicy::kNetworkOnly ||
      policy == HeavyAdUnloadPolicy::kAll) {
    size_t network_threshold =
        heavy_ad_thresholds::kMaxNetworkBytes +
        (use_network_threshold_noise ? heavy_ad_network_threshold_noise_ : 0);

    // Check if the frame meets the network threshold, possible including noise.
    if (network_bytes_ >= network_threshold)
      return HeavyAdStatus::kNetwork;
  }
  return HeavyAdStatus::kNone;
}

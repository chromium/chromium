// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/download/download_stats.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace {

#define ADS_HISTOGRAM(suffix, hist_macro, ad_type, value)                  \
  switch (ad_type) {                                                       \
    case AdsPageLoadMetricsObserver::AD_TYPE_GOOGLE:                       \
      hist_macro("PageLoad.Clients.Ads.Google." suffix, value);            \
      break;                                                               \
    case AdsPageLoadMetricsObserver::AD_TYPE_SUBRESOURCE_FILTER:           \
      hist_macro("PageLoad.Clients.Ads.SubresourceFilter." suffix, value); \
      break;                                                               \
    case AdsPageLoadMetricsObserver::AD_TYPE_ALL:                          \
      hist_macro("PageLoad.Clients.Ads.All." suffix, value);               \
      break;                                                               \
  }

#define RESOURCE_BYTES_HISTOGRAM(suffix, was_cached, value)                \
  if (was_cached) {                                                        \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Cache." suffix, value);   \
  } else {                                                                 \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Network." suffix, value); \
  }

// Finds the RenderFrameHost for the handle, possibly using the FrameTreeNode
// ID directly if the the handle has not been committed.
// NOTE: Unsafe with respect to security privileges.
content::RenderFrameHost* FindFrameMaybeUnsafe(
    content::NavigationHandle* handle) {
  return handle->HasCommitted()
             ? handle->GetRenderFrameHost()
             : handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
                   handle->GetFrameTreeNodeId());
}

bool DetectGoogleAd(content::NavigationHandle* navigation_handle) {
  // Because sub-resource filtering isn't always enabled, and doesn't work
  // well in monitoring mode (no CSS enforcement), it's difficult to identify
  // ads. Google ads are prevalent and easy to track, so we'll start by
  // tracking those. Note that the frame name can be very large, so be careful
  // to avoid full string searches if possible.
  // TODO(jkarlin): Track other ad networks that are easy to identify.

  // In case the navigation aborted, look up the RFH by the Frame Tree Node
  // ID. It returns the committed frame host or the initial frame host for the
  // frame if no committed host exists. Using a previous host is fine because
  // once a frame has an ad we always consider it to have an ad.
  // NOTE: Just used for measuring bytes, does not grant security privileges.
  content::RenderFrameHost* current_frame_host =
      FindFrameMaybeUnsafe(navigation_handle);
  if (current_frame_host) {
    const std::string& frame_name = current_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  const GURL& url = navigation_handle->GetURL();
  return url.host_piece() == "tpc.googlesyndication.com" &&
         base::StartsWith(url.path_piece(), "/safeframe",
                          base::CompareCase::SENSITIVE);
}

bool IsSubframeSameOriginToMainFrame(content::RenderFrameHost* sub_host,
                                     bool use_parent_origin) {
  DCHECK(sub_host);
  content::RenderFrameHost* main_host =
      content::WebContents::FromRenderFrameHost(sub_host)->GetMainFrame();
  if (use_parent_origin)
    sub_host = sub_host->GetParent();
  url::Origin subframe_origin = sub_host->GetLastCommittedOrigin();
  url::Origin mainframe_origin = main_host->GetLastCommittedOrigin();
  return subframe_origin.IsSameOriginWith(mainframe_origin);
}

void RecordDownloadMetrics(blink::DownloadStats::FrameType frame_type,
                           bool has_user_gesture) {
  blink::DownloadStats::GestureType gesture_type =
      has_user_gesture ? blink::DownloadStats::GestureType::kWithGesture
                       : blink::DownloadStats::GestureType::kWithoutGesture;
  blink::DownloadStats::Record(frame_type, gesture_type);
}

using ResourceMimeType = AdsPageLoadMetricsObserver::ResourceMimeType;

}  // namespace

AdsPageLoadMetricsObserver::AdFrameData::AdFrameData(
    FrameTreeNodeId frame_tree_node_id,
    AdTypes ad_types,
    AdOriginStatus origin_status,
    bool frame_navigated)
    : frame_bytes(0u),
      frame_bytes_uncached(0u),
      frame_tree_node_id(frame_tree_node_id),
      ad_types(ad_types),
      origin_status(origin_status),
      frame_navigated(frame_navigated) {}

// static
std::unique_ptr<AdsPageLoadMetricsObserver>
AdsPageLoadMetricsObserver::CreateIfNeeded() {
  if (!base::FeatureList::IsEnabled(features::kAdsFeature))
    return nullptr;
  return std::make_unique<AdsPageLoadMetricsObserver>();
}

AdsPageLoadMetricsObserver::AdsPageLoadMetricsObserver()
    : subresource_observer_(this) {}

AdsPageLoadMetricsObserver::~AdsPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  auto* observer_manager =
      subresource_filter::SubresourceFilterObserverManager::FromWebContents(
          navigation_handle->GetWebContents());
  // |observer_manager| isn't constructed if the feature for subresource
  // filtering isn't enabled.
  if (observer_manager)
    subresource_observer_.Add(observer_manager);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK(ad_frames_data_.empty());

  committed_ = true;

  // The main frame is never considered an ad.
  ad_frames_data_[navigation_handle->GetFrameTreeNodeId()] = nullptr;
  ProcessOngoingNavigationResource(navigation_handle->GetFrameTreeNodeId());
  return CONTINUE_OBSERVING;
}

// Given an ad being triggered for a frame or navigation, get its AdFrameData
// and record it into the appropriate data structures.
void AdsPageLoadMetricsObserver::RecordAdFrameData(
    FrameTreeNodeId ad_id,
    AdTypes ad_types,
    content::RenderFrameHost* ad_host,
    bool frame_navigated) {
  // If an existing subframe is navigating and it was an ad previously that
  // hasn't navigated yet, then we need to update it.
  const auto& id_and_data = ad_frames_data_.find(ad_id);
  AdFrameData* previous_data = nullptr;
  if (id_and_data != ad_frames_data_.end() && id_and_data->second) {
    DCHECK(frame_navigated);
    if (id_and_data->second->frame_navigated) {
      // We need to update the types with any new types that triggered it.
      id_and_data->second->ad_types |= ad_types;
      ProcessOngoingNavigationResource(ad_id);
      return;
    }
    previous_data = id_and_data->second;
  }

  // Determine who the parent frame's ad ancestor is.  If we don't know who it
  // is, return, such as with a frame from a previous navigation.
  content::RenderFrameHost* parent_frame_host =
      ad_host ? ad_host->GetParent() : nullptr;
  const auto& parent_id_and_data =
      parent_frame_host
          ? ad_frames_data_.find(parent_frame_host->GetFrameTreeNodeId())
          : ad_frames_data_.end();
  bool parent_exists = parent_id_and_data != ad_frames_data_.end();
  if (!parent_exists)
    return;

  // This frame is not nested within an ad frame but is itself an ad.
  AdFrameData* ad_data = parent_id_and_data->second;
  if (!ad_data && ad_types.any()) {
    AdOriginStatus origin_status = AdOriginStatus::kUnknown;
    if (ad_host) {
      // For ads triggered on render, their origin is their parent's origin.
      origin_status = IsSubframeSameOriginToMainFrame(ad_host, !frame_navigated)
                          ? AdOriginStatus::kSame
                          : AdOriginStatus::kCross;
    }
    // If data existed already, update it and exit, otherwise, add it.
    if (previous_data) {
      previous_data->ad_types |= ad_types;
      previous_data->origin_status = origin_status;
      previous_data->frame_navigated = frame_navigated;
      return;
    }
    ad_frames_data_storage_.emplace_back(ad_id, ad_types, origin_status,
                                         frame_navigated);
    ad_data = &ad_frames_data_storage_.back();
  }

  // If there was previous data, then we don't want to overwrite this frame.
  if (!previous_data)
    ad_frames_data_[ad_id] = ad_data;
}

// Determine if the frame is part of an existing ad, the root of a new ad, or a
// non-ad frame. Once a frame is labeled as an ad, it is always considered an
// ad, even if it navigates to a non-ad page. This function labels all of a
// page's frames, even those that fail to commit.
void AdsPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  FrameTreeNodeId frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  AdTypes ad_types = DetectAds(navigation_handle);

  // NOTE: Frame look-up only used for determining cross-origin status, not
  // granting security permissions.
  content::RenderFrameHost* ad_host = FindFrameMaybeUnsafe(navigation_handle);

  if (navigation_handle->IsDownload()) {
    blink::DownloadStats::FrameType frame_type =
        IsSubframeSameOriginToMainFrame(ad_host, /*use_parent_origin=*/false)
            ? ad_types.any()
                  ? blink::DownloadStats::FrameType::kSameOriginAdSubframe
                  : blink::DownloadStats::FrameType::kSameOriginNonAdSubframe
            : ad_types.any()
                  ? blink::DownloadStats::FrameType::kCrossOriginAdSubframe
                  : blink::DownloadStats::FrameType::kCrossOriginNonAdSubframe;
    RecordDownloadMetrics(frame_type, navigation_handle->HasUserGesture());
  }

  RecordAdFrameData(frame_tree_node_id, ad_types, ad_host,
                    /*frame_navigated=*/true);
  ProcessOngoingNavigationResource(frame_tree_node_id);
}

void AdsPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  // Main frame navigation
  if (navigation_handle->IsDownload()) {
    RecordDownloadMetrics(blink::DownloadStats::FrameType::kMainFrame,
                          navigation_handle->HasUserGesture());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  if (extra_info.did_commit) {
    if (timing.response_start)
      time_commit_ = timing.navigation_start + *timing.response_start;
    RecordHistograms(extra_info.source_id);
  }

  return STOP_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info) {
  ProcessLoadedResource(extra_request_info);
}

void AdsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.did_commit && timing.response_start)
    time_commit_ = timing.navigation_start + *timing.response_start;
  RecordHistograms(info.source_id);
}

void AdsPageLoadMetricsObserver::OnResourceDataUseObserved(
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources)
    UpdateResource(resource);
}

void AdsPageLoadMetricsObserver::OnSubframeNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    subresource_filter::LoadPolicy load_policy,
    bool is_ad_subframe) {
  // We don't track DISALLOW frames because their resources won't be loaded
  // and therefore would provide bad histogram data. Note that WOULD_DISALLOW
  // is only seen in dry runs.
  if (is_ad_subframe &&
      load_policy != subresource_filter::LoadPolicy::DISALLOW) {
    unfinished_subresource_ad_frames_.insert(
        navigation_handle->GetFrameTreeNodeId());
  }
}

void AdsPageLoadMetricsObserver::OnPageInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (timing.interactive_timing->interactive) {
    time_interactive_ =
        timing.navigation_start + *timing.interactive_timing->interactive;
  }
}

void AdsPageLoadMetricsObserver::OnAdSubframeDetected(
    content::RenderFrameHost* render_frame_host) {
  AdTypes ad_types;
  ad_types.set(AD_TYPE_SUBRESOURCE_FILTER);
  FrameTreeNodeId frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  RecordAdFrameData(frame_tree_node_id, ad_types, render_frame_host,
                    /*frame_navigated=*/false);
}

void AdsPageLoadMetricsObserver::OnSubresourceFilterGoingAway() {
  subresource_observer_.RemoveAll();
}

bool AdsPageLoadMetricsObserver::DetectSubresourceFilterAd(
    FrameTreeNodeId frame_tree_node_id) {
  return unfinished_subresource_ad_frames_.erase(frame_tree_node_id);
}

AdsPageLoadMetricsObserver::AdTypes AdsPageLoadMetricsObserver::DetectAds(
    content::NavigationHandle* navigation_handle) {
  AdTypes ad_types;

  if (DetectGoogleAd(navigation_handle))
    ad_types.set(AD_TYPE_GOOGLE);

  if (DetectSubresourceFilterAd(navigation_handle->GetFrameTreeNodeId()))
    ad_types.set(AD_TYPE_SUBRESOURCE_FILTER);

  return ad_types;
}

void AdsPageLoadMetricsObserver::ProcessLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info) {
  const auto& id_and_data =
      ad_frames_data_.find(extra_request_info.frame_tree_node_id);
  if (id_and_data == ad_frames_data_.end()) {
    if (extra_request_info.resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
        extra_request_info.resource_type == content::RESOURCE_TYPE_SUB_FRAME) {
      // This resource request is the primary resource load for a frame that
      // hasn't yet finished navigating. Hang onto the request info and replay
      // it once the frame finishes navigating.
      ongoing_navigation_resources_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(extra_request_info.frame_tree_node_id),
          std::forward_as_tuple(
              extra_request_info.url, extra_request_info.host_port_pair,
              extra_request_info.frame_tree_node_id,
              extra_request_info.was_cached, extra_request_info.raw_body_bytes,
              extra_request_info.original_network_content_length, nullptr,
              extra_request_info.resource_type, extra_request_info.net_error,
              extra_request_info.load_timing_info
                  ? std::make_unique<net::LoadTimingInfo>(
                        *extra_request_info.load_timing_info)
                  : nullptr));
    } else {
      // This is unexpected, it could be:
      // 1. a resource from a previous navigation that started its resource
      //    load after this page started navigation.
      // 2. possibly a resource from a document.written frame whose frame
      //    failure message has yet to arrive. (uncertain of this)
    }
    return;
  }

  page_bytes_ += extra_request_info.raw_body_bytes;
  if (!extra_request_info.was_cached)
    uncached_page_bytes_ += extra_request_info.raw_body_bytes;

  // Determine if the frame (or its ancestor) is an ad, if so attribute the
  // bytes to the highest ad ancestor.
  AdFrameData* ancestor_data = id_and_data->second;

  if (ancestor_data) {
    ancestor_data->frame_bytes += extra_request_info.raw_body_bytes;
    if (!extra_request_info.was_cached) {
      ancestor_data->frame_bytes_uncached += extra_request_info.raw_body_bytes;
    }
  }
}

AdsPageLoadMetricsObserver::ResourceMimeType
AdsPageLoadMetricsObserver::GetResourceMimeType(
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
  else if (top_level_type.compare("text") == 0 && subtype.compare("css") == 0)
    return ResourceMimeType::kCss;
  else if (top_level_type.compare("text") == 0 && subtype.compare("html") == 0)
    return ResourceMimeType::kHtml;
  else
    return ResourceMimeType::kOther;
}

void AdsPageLoadMetricsObserver::UpdateResource(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  auto it = page_resources_.find(resource->request_id);
  // A new resource has been observed.
  if (it == page_resources_.end())
    total_number_page_resources_++;

  page_resource_bytes_ += resource->delta_bytes;
  if (resource->reported_as_ad_resource) {
    // If the resource had already started loading, and is now labeled as an ad,
    // but was not before, we need to account for all the previously received
    // bytes.
    bool is_new_ad =
        (it != page_resources_.end()) && !it->second->reported_as_ad_resource;
    int unaccounted_ad_bytes =
        is_new_ad ? resource->received_data_length - resource->delta_bytes : 0;
    page_ad_resource_bytes_ += resource->delta_bytes + unaccounted_ad_bytes;
    if (resource->is_main_frame_resource) {
      page_main_frame_ad_resource_bytes_ +=
          resource->delta_bytes + unaccounted_ad_bytes;
    }
    if (!time_interactive_.is_null()) {
      page_ad_resource_bytes_since_interactive_ +=
          resource->delta_bytes + unaccounted_ad_bytes;
    }
    ResourceMimeType mime_type = GetResourceMimeType(resource);
    if (mime_type == ResourceMimeType::kVideo)
      page_ad_video_bytes_ += resource->delta_bytes + unaccounted_ad_bytes;
    if (mime_type == ResourceMimeType::kJavascript)
      page_ad_javascript_bytes_ += resource->delta_bytes + unaccounted_ad_bytes;
  }

  // Update resource map.
  if (resource->is_complete) {
    RecordResourceHistograms(resource);
    if (it != page_resources_.end())
      page_resources_.erase(it);
  } else {
    // Must clone resource so it will be accessible when the observer is
    // destroyed.
    if (it != page_resources_.end()) {
      it->second = resource->Clone();
    } else {
      page_resources_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(resource->request_id),
                              std::forward_as_tuple(resource->Clone()));
    }
  }
}

void AdsPageLoadMetricsObserver::RecordResourceMimeHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  int64_t data_length = resource->was_fetched_via_cache
                            ? resource->encoded_body_length
                            : resource->received_data_length;
  ResourceMimeType mime_type = GetResourceMimeType(resource);
  if (mime_type == ResourceMimeType::kImage) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Image", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kJavascript) {
    RESOURCE_BYTES_HISTOGRAM("Mime.JS", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kVideo) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Video", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kCss) {
    RESOURCE_BYTES_HISTOGRAM("Mime.CSS", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kHtml) {
    RESOURCE_BYTES_HISTOGRAM("Mime.HTML", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kOther) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Other", resource->was_fetched_via_cache,
                             data_length);
  }
}

void AdsPageLoadMetricsObserver::RecordResourceHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  int64_t data_length = resource->was_fetched_via_cache
                            ? resource->encoded_body_length
                            : resource->received_data_length;
  if (resource->is_main_frame_resource && resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.AdResource",
                             resource->was_fetched_via_cache, data_length);
  } else if (resource->is_main_frame_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.VanillaResource",
                             resource->was_fetched_via_cache, data_length);
  } else if (resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Subframe.AdResource",
                             resource->was_fetched_via_cache, data_length);
  } else {
    RESOURCE_BYTES_HISTOGRAM("Subframe.VanillaResource",
                             resource->was_fetched_via_cache, data_length);
  }

  // Only report sizes by mime type for ad resources.
  if (resource->reported_as_ad_resource)
    RecordResourceMimeHistograms(resource);
}

void AdsPageLoadMetricsObserver::RecordPageResourceTotalHistograms(
    ukm::SourceId source_id) {
  // Only records histograms on pages that have some ad bytes.
  if (page_ad_resource_bytes_ == 0)
    return;
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Total",
                       page_resource_bytes_);
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Ads",
                       page_ad_resource_bytes_);
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.TopLevelAds",
                       page_main_frame_ad_resource_bytes_);
  size_t unfinished_bytes = 0;
  for (auto const& kv : page_resources_)
    unfinished_bytes += kv.second->received_data_length;
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Unfinished",
                       unfinished_bytes);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::AdPageLoad builder(source_id);
  builder.SetTotalBytes(page_resource_bytes_ >> 10)
      .SetAdBytes(page_ad_resource_bytes_ >> 10)
      .SetAdJavascriptBytes(page_ad_javascript_bytes_ >> 10)
      .SetAdVideoBytes(page_ad_video_bytes_ >> 10);
  base::Time current_time = base::Time::Now();
  if (!time_commit_.is_null()) {
    int time_since_commit = (current_time - time_commit_).InMicroseconds();
    if (time_since_commit > 0) {
      int ad_kbps_from_commit =
          (page_ad_resource_bytes_ >> 10) * 1000 * 1000 / time_since_commit;
      builder.SetAdBytesPerSecond(ad_kbps_from_commit);
    }
  }
  if (!time_interactive_.is_null()) {
    int time_since_interactive =
        (current_time - time_interactive_).InMicroseconds();
    if (time_since_interactive > 0) {
      int ad_kbps_since_interactive =
          (page_ad_resource_bytes_since_interactive_ >> 10) * 1000 * 1000 /
          time_since_interactive;
      builder.SetAdBytesPerSecondAfterInteractive(ad_kbps_since_interactive);
    }
  }
  builder.Record(ukm_recorder->Get());
}

void AdsPageLoadMetricsObserver::RecordHistograms(ukm::SourceId source_id) {
  RecordHistogramsForType(AD_TYPE_GOOGLE);
  RecordHistogramsForType(AD_TYPE_SUBRESOURCE_FILTER);
  RecordHistogramsForType(AD_TYPE_ALL);
  RecordPageResourceTotalHistograms(source_id);
  for (auto const& kv : page_resources_)
    RecordResourceHistograms(kv.second);
}

void AdsPageLoadMetricsObserver::RecordHistogramsForType(int ad_type) {
  if (page_bytes_ == 0)
    return;

  int non_zero_ad_frames = 0;
  size_t total_ad_frame_bytes = 0;
  size_t uncached_ad_frame_bytes = 0;

  for (const AdFrameData& ad_frame_data : ad_frames_data_storage_) {
    if (ad_frame_data.frame_bytes == 0)
      continue;

    // If this isn't the type of ad we're looking for, move on to the next.
    if (ad_type != AD_TYPE_ALL && !ad_frame_data.ad_types.test(ad_type))
      continue;

    non_zero_ad_frames += 1;
    total_ad_frame_bytes += ad_frame_data.frame_bytes;

    uncached_ad_frame_bytes += ad_frame_data.frame_bytes_uncached;
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Total", PAGE_BYTES_HISTOGRAM,
                  ad_type, ad_frame_data.frame_bytes);
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Network", PAGE_BYTES_HISTOGRAM,
                  ad_type, ad_frame_data.frame_bytes_uncached);
    ADS_HISTOGRAM(
        "Bytes.AdFrames.PerFrame.PercentNetwork", UMA_HISTOGRAM_PERCENTAGE,
        ad_type,
        ad_frame_data.frame_bytes_uncached * 100 / ad_frame_data.frame_bytes);
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.OriginStatus",
                  UMA_HISTOGRAM_ENUMERATION, ad_type,
                  ad_frame_data.origin_status);
  }

  // TODO(ericrobinson): Consider renaming this to match
  //   'FrameCounts.AdFrames.PerFrame.OriginStatus'.
  ADS_HISTOGRAM("FrameCounts.AnyParentFrame.AdFrames",
                UMA_HISTOGRAM_COUNTS_1000, ad_type, non_zero_ad_frames);

  // Don't post UMA for pages that don't have ads.
  if (non_zero_ad_frames == 0)
    return;

  ADS_HISTOGRAM("Bytes.NonAdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM,
                ad_type, page_bytes_ - total_ad_frame_bytes);

  ADS_HISTOGRAM("Bytes.FullPage.Total", PAGE_BYTES_HISTOGRAM, ad_type,
                page_bytes_);
  ADS_HISTOGRAM("Bytes.FullPage.Network", PAGE_BYTES_HISTOGRAM, ad_type,
                uncached_page_bytes_);

  if (page_bytes_) {
    ADS_HISTOGRAM("Bytes.FullPage.Total.PercentAds", UMA_HISTOGRAM_PERCENTAGE,
                  ad_type, total_ad_frame_bytes * 100 / page_bytes_);
  }
  if (uncached_page_bytes_ > 0) {
    ADS_HISTOGRAM("Bytes.FullPage.Network.PercentAds", UMA_HISTOGRAM_PERCENTAGE,
                  ad_type,
                  uncached_ad_frame_bytes * 100 / uncached_page_bytes_);
  }

  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM, ad_type,
                total_ad_frame_bytes);
  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Network", PAGE_BYTES_HISTOGRAM,
                ad_type, uncached_ad_frame_bytes);

  if (total_ad_frame_bytes) {
    ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.PercentNetwork",
                  UMA_HISTOGRAM_PERCENTAGE, ad_type,
                  uncached_ad_frame_bytes * 100 / total_ad_frame_bytes);
  }
}

void AdsPageLoadMetricsObserver::ProcessOngoingNavigationResource(
    FrameTreeNodeId frame_tree_node_id) {
  const auto& frame_id_and_request =
      ongoing_navigation_resources_.find(frame_tree_node_id);
  if (frame_id_and_request == ongoing_navigation_resources_.end())
    return;

  ProcessLoadedResource(frame_id_and_request->second);
  ongoing_navigation_resources_.erase(frame_id_and_request);
}

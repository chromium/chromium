// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/resource_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/resource_tracker.h"

namespace {

#define RESOURCE_BYTES_HISTOGRAM(suffix, was_cached, value)                \
  if (was_cached) {                                                        \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Cache2." suffix, value);  \
  } else {                                                                 \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Network." suffix, value); \
  }

}  // namespace

ResourceMetricsObserver::ResourceMetricsObserver() {}

ResourceMetricsObserver::~ResourceMetricsObserver() {}

void ResourceMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* content,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    if (resource->is_complete)
      RecordResourceHistograms(resource);
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ResourceMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO(johnidel): This logic was maintained when resource metrics were moved
  // out of the AdsPageLoadMetricsObserver. These metrics don't need to stop
  // being reported when backgrounded.
  if (GetDelegate().DidCommit()) {
    OnComplete(timing);
  }

  return STOP_OBSERVING;
}

void ResourceMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  for (auto const& kv :
       GetDelegate().GetResourceTracker().unfinished_resources())
    RecordResourceHistograms(kv.second);
}

void ResourceMetricsObserver::RecordResourceMimeHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  bool was_cached =
      resource->cache_type != page_load_metrics::mojom::CacheType::kNotCached;
  int64_t data_length = was_cached ? resource->encoded_body_length
                                   : resource->received_data_length;
  ResourceMimeType mime_type = FrameData::GetResourceMimeType(resource);
  if (mime_type == ResourceMimeType::kImage) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Image", was_cached, data_length);
  } else if (mime_type == ResourceMimeType::kJavascript) {
    RESOURCE_BYTES_HISTOGRAM("Mime.JS", was_cached, data_length);
  } else if (mime_type == ResourceMimeType::kVideo) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Video", was_cached, data_length);
  } else if (mime_type == ResourceMimeType::kCss) {
    RESOURCE_BYTES_HISTOGRAM("Mime.CSS", was_cached, data_length);
  } else if (mime_type == ResourceMimeType::kHtml) {
    RESOURCE_BYTES_HISTOGRAM("Mime.HTML", was_cached, data_length);
  } else if (mime_type == ResourceMimeType::kOther) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Other", was_cached, data_length);
  }
}

void ResourceMetricsObserver::RecordResourceHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  bool was_cached =
      resource->cache_type != page_load_metrics::mojom::CacheType::kNotCached;
  int64_t data_length = was_cached ? resource->encoded_body_length
                                   : resource->received_data_length;
  if (resource->is_main_frame_resource && resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.AdResource", was_cached, data_length);
  } else if (resource->is_main_frame_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.VanillaResource", was_cached,
                             data_length);
  } else if (resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Subframe.AdResource", was_cached, data_length);
  } else {
    RESOURCE_BYTES_HISTOGRAM("Subframe.VanillaResource", was_cached,
                             data_length);
  }

  // Only report sizes by mime type for ad resources.
  if (resource->reported_as_ad_resource)
    RecordResourceMimeHistograms(resource);
}

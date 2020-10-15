// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_prefetch_metrics_collector.h"

#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// This intentionally does not use the same logic as the TabHelper handling of
// mainframe prefetches since the returned metric is used in a different place
// and context that is not guaranteed to match perfectly.
IsolatedPrerenderPrefetchStatus GetStatusOfPrefetch(
    network::mojom::URLResponseHead* head,
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    return IsolatedPrerenderPrefetchStatus::kPrefetchFailedNetError;
  }

  if (!head || !head->headers) {
    return IsolatedPrerenderPrefetchStatus::kPrefetchFailedNetError;
  }

  int response_code = head->headers->response_code();
  if (response_code < net::HTTP_OK ||
      response_code >= net::HTTP_MULTIPLE_CHOICES) {
    return IsolatedPrerenderPrefetchStatus::kPrefetchFailedNon2XX;
  }
  return IsolatedPrerenderPrefetchStatus::kPrefetchSuccessful;
}

}  // namespace

IsolatedPrerenderPrefetchMetricsCollector::PrefetchMetric::PrefetchMetric() =
    default;
IsolatedPrerenderPrefetchMetricsCollector::PrefetchMetric::PrefetchMetric(
    const PrefetchMetric& copy) = default;
IsolatedPrerenderPrefetchMetricsCollector::PrefetchMetric::~PrefetchMetric() =
    default;

IsolatedPrerenderPrefetchMetricsCollector::
    IsolatedPrerenderPrefetchMetricsCollector(
        base::TimeTicks navigation_start_time,
        ukm::SourceId ukm_source_id)
    : navigation_start_time_(navigation_start_time),
      ukm_source_id_(ukm_source_id) {}

IsolatedPrerenderPrefetchMetricsCollector::
    ~IsolatedPrerenderPrefetchMetricsCollector() {
  for (auto entry : resources_by_url_) {
    const PrefetchMetric& metric = entry.second;

    ukm::builders::PrefetchProxy_PrefetchedResource builder(ukm_source_id_);
    builder.SetResourceType(metric.is_mainframe ? 1 : 2);
    builder.SetStatus(static_cast<int>(metric.status));

    if (metric.data_length) {
      builder.SetDataLength(
          ukm::GetExponentialBucketMinForBytes(*metric.data_length));
    }
    if (metric.fetch_duration) {
      builder.SetFetchDurationMS(metric.fetch_duration->InMilliseconds());
    }

    if (metric.was_clicked) {
      builder.SetLinkClicked(*metric.was_clicked);
    }
    if (metric.link_position) {
      builder.SetLinkPosition(*metric.link_position);
    }
    if (metric.navigation_start_to_fetch_start) {
      builder.SetNavigationStartToFetchStartMS(
          metric.navigation_start_to_fetch_start->InMilliseconds());
    }
    if (metric.filtering_result) {
      builder.SetISPFilteringStatus(static_cast<int>(*metric.filtering_result));
    }

    builder.Record(ukm::UkmRecorder::Get());
  }
}

void IsolatedPrerenderPrefetchMetricsCollector::MapMainframeToSubresource(
    const GURL& mainframe_url,
    const GURL& subresource_url) {
  auto mainframe_mapping = subresources_by_mainframe_.find(mainframe_url);
  if (mainframe_mapping == subresources_by_mainframe_.end()) {
    subresources_by_mainframe_.emplace(mainframe_url,
                                       std::set<GURL>{subresource_url});
  } else {
    mainframe_mapping->second.emplace(subresource_url);
  }
}

base::Optional<IsolatedPrerenderPrefetchStatus>
IsolatedPrerenderPrefetchMetricsCollector::GetStatusOfMainframe(
    const GURL& url) const {
  auto mainframe_entry = resources_by_url_.find(url);
  if (mainframe_entry != resources_by_url_.end()) {
    return mainframe_entry->second.status;
  }
  NOTREACHED() << "Unknown mainframe url";
  return base::nullopt;
}

base::Optional<size_t>
IsolatedPrerenderPrefetchMetricsCollector::GetLinkPositionOfMainframe(
    const GURL& url) const {
  auto mainframe_entry = resources_by_url_.find(url);
  if (mainframe_entry != resources_by_url_.end()) {
    return mainframe_entry->second.link_position;
  }
  NOTREACHED() << "Unknown mainframe url";
  return base::nullopt;
}

void IsolatedPrerenderPrefetchMetricsCollector::OnMainframeResourceNotEligible(
    const GURL& url,
    size_t prediction_position,
    IsolatedPrerenderPrefetchStatus status) {
  PrefetchMetric metric;
  metric.status = status;
  metric.is_mainframe = true;
  metric.link_position = prediction_position;
  metric.was_clicked = false;

  resources_by_url_.emplace(url, metric);
}

void IsolatedPrerenderPrefetchMetricsCollector::OnSubresourceNotEligible(
    const GURL& mainframe_url,
    const GURL& subresource_url,
    IsolatedPrerenderPrefetchStatus status) {
  PrefetchMetric metric;
  metric.status = status;
  metric.is_mainframe = false;
  metric.link_position = GetLinkPositionOfMainframe(mainframe_url);
  metric.was_clicked = false;

  resources_by_url_.emplace(subresource_url, metric);

  MapMainframeToSubresource(mainframe_url, subresource_url);
}

void IsolatedPrerenderPrefetchMetricsCollector::OnMainframeResourcePrefetched(
    const GURL& url,
    size_t prediction_position,
    network::mojom::URLResponseHeadPtr head,
    const network::URLLoaderCompletionStatus& status) {
  PrefetchMetric metric;
  metric.status = GetStatusOfPrefetch(head.get(), status);
  metric.is_mainframe = true;
  metric.link_position = prediction_position;
  metric.data_length = status.encoded_data_length;
  metric.was_clicked = false;
  if (head) {
    metric.navigation_start_to_fetch_start =
        head->load_timing.request_start - navigation_start_time_;
    metric.fetch_duration =
        status.completion_time - head->load_timing.request_start;
  }

  resources_by_url_.emplace(url, metric);
}

void IsolatedPrerenderPrefetchMetricsCollector::OnSubresourcePrefetched(
    const GURL& mainframe_url,
    const GURL& subresource_url,
    network::mojom::URLResponseHeadPtr head,
    const network::URLLoaderCompletionStatus& status) {
  PrefetchMetric metric;
  metric.status = GetStatusOfPrefetch(head.get(), status);
  metric.is_mainframe = false;
  metric.data_length = status.encoded_data_length;
  metric.was_clicked = false;
  metric.link_position = GetLinkPositionOfMainframe(mainframe_url);
  if (head) {
    metric.navigation_start_to_fetch_start =
        head->load_timing.request_start - navigation_start_time_;
    metric.fetch_duration =
        status.completion_time - head->load_timing.request_start;
  }

  resources_by_url_.emplace(subresource_url, metric);

  MapMainframeToSubresource(mainframe_url, subresource_url);
}

void IsolatedPrerenderPrefetchMetricsCollector::OnMainframeNavigatedTo(
    const GURL& url) {
  // Set was clicked on the mainframe.
  auto mainframe_entry = resources_by_url_.find(url);
  if (mainframe_entry != resources_by_url_.end()) {
    mainframe_entry->second.was_clicked = true;
  }

  // Set was clicked on all subresources.
  auto subresource_mapping = subresources_by_mainframe_.find(url);
  if (subresource_mapping == subresources_by_mainframe_.end()) {
    return;
  }

  for (const GURL& subresource : subresource_mapping->second) {
    auto entry = resources_by_url_.find(subresource);
    if (entry == resources_by_url_.end()) {
      NOTREACHED() << "Mapped subresource not found in resources_by_url_";
      continue;
    }

    entry->second.was_clicked = true;
  }
}

void IsolatedPrerenderPrefetchMetricsCollector::
    OnMainframeNavigationProbeResult(const GURL& url,
                                     IsolatedPrerenderProbeResult result) {
  auto mainframe_entry = resources_by_url_.find(url);
  if (mainframe_entry == resources_by_url_.end()) {
    return;
  }

  switch (result) {
    case IsolatedPrerenderProbeResult::kNoProbing:
      mainframe_entry->second.status =
          IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbe;
      break;
    case IsolatedPrerenderProbeResult::kDNSProbeSuccess:
    case IsolatedPrerenderProbeResult::kTLSProbeSuccess:
      mainframe_entry->second.status =
          IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccess;
      break;
    case IsolatedPrerenderProbeResult::kTLSProbeFailure:
    case IsolatedPrerenderProbeResult::kDNSProbeFailure:
      mainframe_entry->second.status =
          IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailed;
      break;
  }

  mainframe_entry->second.filtering_result = result;

  // Also update the filtering result field on all associated subresources.
  auto mapped_subresources = subresources_by_mainframe_.find(url);
  if (mapped_subresources == subresources_by_mainframe_.end()) {
    return;
  }

  for (const GURL& subresource_url : mapped_subresources->second) {
    auto subresource_entry = resources_by_url_.find(subresource_url);
    if (subresource_entry == resources_by_url_.end()) {
      continue;
    }
    subresource_entry->second.filtering_result = result;
  }
}

void IsolatedPrerenderPrefetchMetricsCollector::OnCachedSubresourceUsed(
    const GURL& mainframe_url,
    const GURL& subresource_url) {
  auto entry_iter = resources_by_url_.find(subresource_url);
  if (entry_iter == resources_by_url_.end()) {
    NOTREACHED() << "Used a resource that wasn't reported as being fetched";
    return;
  }

  base::Optional<IsolatedPrerenderPrefetchStatus> status =
      GetStatusOfMainframe(mainframe_url);
  if (status) {
    entry_iter->second.status = *status;
  }
}

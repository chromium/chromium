// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_data_collector.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/predictors/loading_stats_collector.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

bool g_allow_port_in_urls = true;

// Sorted by decreasing likelihood according to HTTP archive.
const char* const kFontMimeTypes[] = {"font/woff2",
                                      "application/x-font-woff",
                                      "application/font-woff",
                                      "application/font-woff2",
                                      "font/x-woff",
                                      "application/x-font-ttf",
                                      "font/woff",
                                      "font/ttf",
                                      "application/x-font-otf",
                                      "x-font/woff",
                                      "application/font-sfnt",
                                      "application/font-ttf"};

// Determines the request destination from the mime type, defaulting to the
// |fallback| if the destination could not be determined.
network::mojom::RequestDestination GetRequestDestinationFromMimeType(
    const std::string& mime_type,
    network::mojom::RequestDestination fallback) {
  if (mime_type.empty()) {
    return fallback;
  } else if (blink::IsSupportedImageMimeType(mime_type)) {
    return network::mojom::RequestDestination::kImage;
  } else if (blink::IsSupportedJavascriptMimeType(mime_type)) {
    return network::mojom::RequestDestination::kScript;
  } else if (net::MatchesMimeType("text/css", mime_type)) {
    return network::mojom::RequestDestination::kStyle;
  } else {
    bool found = base::ranges::any_of(
        kFontMimeTypes, [&mime_type](const std::string& mime) {
          return net::MatchesMimeType(mime, mime_type);
        });
    if (found)
      return network::mojom::RequestDestination::kFont;
  }
  return fallback;
}

// Determines the resource type from the declared one, falling back to MIME
// type detection when it is not explicit.
network::mojom::RequestDestination GetRequestDestination(
    network::mojom::RequestDestination destination,
    const std::string& mime_type) {
  // Restricts empty request destination (e.g. prefetch, prerender, fetch,
  // xhr etc) to a small set of mime types, because these destination types
  // don't communicate how the resources will be used.
  if (destination == network::mojom::RequestDestination::kEmpty) {
    return GetRequestDestinationFromMimeType(
        mime_type, network::mojom::RequestDestination::kEmpty);
  }
  return destination;
}

}  // namespace

OriginRequestSummary::OriginRequestSummary() = default;
OriginRequestSummary::OriginRequestSummary(const OriginRequestSummary& other) =
    default;
OriginRequestSummary::~OriginRequestSummary() = default;

PageRequestSummary::PageRequestSummary(ukm::SourceId ukm_source_id,
                                       const GURL& main_frame_url,
                                       base::TimeTicks navigation_started)
    : ukm_source_id(ukm_source_id),
      main_frame_url(main_frame_url),
      initial_url(main_frame_url),
      navigation_started(navigation_started) {}

PageRequestSummary::PageRequestSummary(const PageRequestSummary& other) =
    default;

void PageRequestSummary::UpdateOrAddResource(
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  auto should_record_resource_load =
      ShouldRecordResourceLoad(resource_load_info);
  switch (should_record_resource_load) {
    case ShouldRecordResourceLoadResult::kNo:
      return;
    case ShouldRecordResourceLoadResult::kLowPriority:
    case ShouldRecordResourceLoadResult::kYes:
      const bool is_low_priority = should_record_resource_load ==
                                   ShouldRecordResourceLoadResult::kLowPriority;
      for (const auto& redirect_info : resource_load_info.redirect_info_chain) {
        UpdateOrAddToOrigins(redirect_info->origin_of_new_url,
                             redirect_info->network_info, is_low_priority);
      }
      UpdateOrAddToOrigins(url::Origin::Create(resource_load_info.final_url),
                           resource_load_info.network_info, is_low_priority);
      const GURL final_url =
          net::SimplifyUrlForRequest(resource_load_info.final_url);
      if (is_low_priority) {
        low_priority_subresource_urls.insert(final_url);
      } else {
        subresource_urls.insert(final_url);
      }
      return;
  }
}

void PageRequestSummary::AddPreconnectAttempt(const GURL& preconnect_url) {
  if (main_frame_load_complete) {
    return;
  }
  url::Origin preconnect_origin = url::Origin::Create(preconnect_url);
  if (preconnect_origin == url::Origin::Create(main_frame_url)) {
    // Do not count preconnect to main frame origin in number of origins
    // preconnected to.
    return;
  }
  preconnect_origins.insert(preconnect_origin);
}

void PageRequestSummary::AddPrefetchAttempt(const GURL& prefetch_url) {
  if (main_frame_load_complete) {
    return;
  }
  prefetch_urls.insert(prefetch_url);

  if (!first_prefetch_initiated)
    first_prefetch_initiated = base::TimeTicks::Now();
}

void PageRequestSummary::MainFrameLoadComplete() {
  main_frame_load_complete = true;
}

PageRequestSummary::ShouldRecordResourceLoadResult
PageRequestSummary::ShouldRecordResourceLoad(
    const blink::mojom::ResourceLoadInfo& resource_load_info) const {
  const GURL& url = resource_load_info.final_url;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return kNo;
  }

  if (!g_allow_port_in_urls && url.has_port()) {
    return kNo;
  }

  if (!IsHandledResourceType(resource_load_info.request_destination,
                             resource_load_info.mime_type)) {
    return kNo;
  }

  if (resource_load_info.method != "GET") {
    return kNo;
  }

  if (main_frame_load_complete) {
    return kLowPriority;
  }

  // Guard behind feature: All delayable requests are considered low priority.
  if (base::FeatureList::IsEnabled(
          features::kLoadingOnlyLearnHighPriorityResources) &&
      resource_load_info.request_priority < net::MEDIUM) {
    return kLowPriority;
  }

  return kYes;
}

// static
bool PageRequestSummary::IsHandledResourceType(
    network::mojom::RequestDestination destination,
    const std::string& mime_type) {
  network::mojom::RequestDestination actual_destination =
      GetRequestDestination(destination, mime_type);
  return actual_destination == network::mojom::RequestDestination::kDocument ||
         actual_destination == network::mojom::RequestDestination::kStyle ||
         actual_destination == network::mojom::RequestDestination::kScript ||
         actual_destination == network::mojom::RequestDestination::kImage ||
         actual_destination == network::mojom::RequestDestination::kFont;
}

void PageRequestSummary::UpdateOrAddToOrigins(
    const url::Origin& origin,
    const blink::mojom::CommonNetworkInfoPtr& network_info,
    bool is_low_priority) {
  if (origin.opaque())
    return;

  if (is_low_priority) {
    low_priority_origins.insert(origin);
    return;
  }

  auto it = origins.find(origin);
  if (it == origins.end()) {
    OriginRequestSummary summary;
    summary.origin = origin;
    summary.first_occurrence = origins.size();
    it = origins.insert({origin, summary}).first;
  }

  it->second.always_access_network |= network_info->always_access_network;
  it->second.accessed_network |= network_info->network_accessed;
}

PageRequestSummary::~PageRequestSummary() = default;

// static
void LoadingDataCollector::SetAllowPortInUrlsForTesting(bool state) {
  g_allow_port_in_urls = state;
}

LoadingDataCollector::LoadingDataCollector(
    ResourcePrefetchPredictor* predictor,
    predictors::LoadingStatsCollector* stats_collector,
    const LoadingPredictorConfig& config)
    : predictor_(predictor),
      stats_collector_(stats_collector),
      config_(config) {}

LoadingDataCollector::~LoadingDataCollector() = default;

void LoadingDataCollector::RecordStartNavigation(
    NavigationId navigation_id,
    ukm::SourceId ukm_source_id,
    const GURL& main_frame_url,
    base::TimeTicks creation_time) {
  CleanupAbandonedNavigations();

  // New empty navigation entry.
  inflight_navigations_.emplace(
      navigation_id, std::make_unique<PageRequestSummary>(
                         ukm_source_id, main_frame_url, creation_time));
}

void LoadingDataCollector::RecordFinishNavigation(
    NavigationId navigation_id,
    const GURL& new_main_frame_url,
    bool is_error_page) {
  if (is_error_page) {
    inflight_navigations_.erase(navigation_id);
    return;
  }

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it != inflight_navigations_.end()) {
    nav_it->second->main_frame_url = new_main_frame_url;
    nav_it->second->navigation_committed = base::TimeTicks::Now();
  }
}

void LoadingDataCollector::RecordResourceLoadComplete(
    NavigationId navigation_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  auto& page_request_summary = *nav_it->second;
  page_request_summary.UpdateOrAddResource(resource_load_info);
}

void LoadingDataCollector::RecordPreconnectInitiated(
    NavigationId navigation_id,
    const GURL& preconnect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  auto& page_request_summary = *nav_it->second;
  page_request_summary.AddPreconnectAttempt(preconnect_url);
}

void LoadingDataCollector::RecordPrefetchInitiated(NavigationId navigation_id,
                                                   const GURL& prefetch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  auto& page_request_summary = *nav_it->second;
  page_request_summary.AddPrefetchAttempt(prefetch_url);
}

void LoadingDataCollector::RecordMainFrameLoadComplete(
    NavigationId navigation_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Initialize |predictor_| no matter whether the |navigation_id| is present in
  // |inflight_navigations_|. This is the case for NTP and about:blank pages,
  // for example.
  if (predictor_)
    predictor_->StartInitialization();

  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end())
    return;

  PageRequestSummary& summary = *nav_it->second;
  summary.MainFrameLoadComplete();

  if (predictor_)
    predictor_->RecordPageRequestSummary(summary);
}

void LoadingDataCollector::RecordPageDestroyed(
    NavigationId navigation_id,
    const std::optional<OptimizationGuidePrediction>&
        optimization_guide_prediction) {
  auto nav_it = inflight_navigations_.find(navigation_id);
  if (nav_it == inflight_navigations_.end()) {
    return;
  }

  std::unique_ptr<PageRequestSummary> summary = std::move(nav_it->second);
  CHECK(summary->navigation_committed.has_value());
  inflight_navigations_.erase(nav_it);
  if (stats_collector_) {
    stats_collector_->RecordPageRequestSummary(*summary,
                                               optimization_guide_prediction);
  }
}

void LoadingDataCollector::CleanupAbandonedNavigations() {
  if (stats_collector_)
    stats_collector_->CleanupAbandonedStats();

  static const base::TimeDelta max_navigation_age =
      base::Seconds(config_.max_navigation_lifetime_seconds);

  base::TimeTicks time_now = base::TimeTicks::Now();
  for (auto it = inflight_navigations_.begin();
       it != inflight_navigations_.end();) {
    if (time_now - it->second->navigation_started > max_navigation_age &&
        !it->second->navigation_committed) {
      it = inflight_navigations_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace predictors

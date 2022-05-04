// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_request.h"

#include "base/trace_event/base_tracing.h"
#include "chrome/browser/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"
#include "net/base/load_flags.h"

StreamingSearchPrefetchRequest::StreamingSearchPrefetchRequest(
    const GURL& prefetch_url,
    bool navigation_prefetch,
    base::OnceCallback<void(bool)> report_error_callback)
    : BaseSearchPrefetchRequest(prefetch_url,
                                navigation_prefetch,
                                std::move(report_error_callback)) {}

StreamingSearchPrefetchRequest::~StreamingSearchPrefetchRequest() = default;

void StreamingSearchPrefetchRequest::StartPrefetchRequestInternal(
    Profile* profile,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::OnceCallback<void(bool)> report_error_callback) {
  TRACE_EVENT0("loading",
               "StreamingSearchPrefetchRequest::StartPrefetchRequestInternal");
  profile_ = profile;
  network_traffic_annotation_ =
      std::make_unique<net::NetworkTrafficAnnotationTag>(
          network_traffic_annotation);
  prefetch_url_ = resource_request->url;
  if (SearchPrefetchUsesNetworkCache()) {
    resource_request->load_flags =
        resource_request->load_flags | net::LOAD_PREFETCH;
  }
  streaming_url_loader_ = std::make_unique<StreamingSearchPrefetchURLLoader>(
      this, profile, navigation_prefetch_, std::move(resource_request),
      network_traffic_annotation, std::move(report_error_callback));
}

std::unique_ptr<SearchPrefetchURLLoader>
StreamingSearchPrefetchRequest::TakeSearchPrefetchURLLoader() {
  streaming_url_loader_->ClearOwnerPointer();
  if (SearchPrefetchUsesNetworkCache()) {
    auto loader = std::make_unique<CacheAliasSearchPrefetchURLLoader>(
        profile_, *network_traffic_annotation_, prefetch_url_,
        std::move(streaming_url_loader_));
    return std::move(loader);
  }
  return std::move(streaming_url_loader_);
}

void StreamingSearchPrefetchRequest::StopPrefetch() {
  streaming_url_loader_.reset();
}

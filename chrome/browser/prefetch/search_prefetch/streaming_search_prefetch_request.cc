// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_request.h"

#include "chrome/browser/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"

StreamingSearchPrefetchRequest::StreamingSearchPrefetchRequest(
    const GURL& prefetch_url,
    base::OnceClosure report_error_callback)
    : BaseSearchPrefetchRequest(prefetch_url,
                                std::move(report_error_callback)) {}

StreamingSearchPrefetchRequest::~StreamingSearchPrefetchRequest() = default;

void StreamingSearchPrefetchRequest::StartPrefetchRequestInternal(
    Profile* profile,
    std::unique_ptr<network::ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation) {
  streaming_url_loader_ = std::make_unique<StreamingSearchPrefetchURLLoader>(
      this, profile, std::move(resource_request), network_traffic_annotation);
}

std::unique_ptr<SearchPrefetchURLLoader>
StreamingSearchPrefetchRequest::TakeSearchPrefetchURLLoader() {
  streaming_url_loader_->ClearOwnerPointer();
  return std::move(streaming_url_loader_);
}

void StreamingSearchPrefetchRequest::StopPrefetch() {
  streaming_url_loader_.reset();
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_keep_alive_request_tracker.h"

#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "services/network/public/cpp/resource_request.h"

// static
std::unique_ptr<SearchPrefetchKeepAliveRequestTracker>
SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
    const network::ResourceRequest& request) {
  if (!IsSearchPrefetchBeaconLoggingEnabled(request.url)) {
    return nullptr;
  }
  // TODO(https://crbug.com/413557424): Create a tracker if the given `request`
  // is initiated by search prefetch resource.
  return nullptr;
}

SearchPrefetchKeepAliveRequestTracker::SearchPrefetchKeepAliveRequestTracker(
    const network::ResourceRequest& request)
    // TODO(https://crbug.com/413557424): Determine the RequestType by
    // retrieving `request`.
    : content::KeepAliveRequestTracker(
          content::KeepAliveRequestTracker::RequestType::kFetch) {}

SearchPrefetchKeepAliveRequestTracker::
    ~SearchPrefetchKeepAliveRequestTracker() = default;

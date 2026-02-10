// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_keep_alive_request_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "services/network/public/cpp/resource_request.h"

// static
std::unique_ptr<SearchPrefetchKeepAliveRequestTracker>
SearchPrefetchKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context) {
  if (!IsSearchPrefetchBeaconLoggingEnabled(request.url, browser_context)) {
    return nullptr;
  }
  return base::WrapUnique(new SearchPrefetchKeepAliveRequestTracker(request));
}

SearchPrefetchKeepAliveRequestTracker::SearchPrefetchKeepAliveRequestTracker(
    const network::ResourceRequest& request)
    // TODO(https://crbug.com/413557424): Determine the RequestType by
    // retrieving `request`.
    : content::KeepAliveRequestTracker(
          content::KeepAliveRequestTracker::RequestType::kFetch) {}

SearchPrefetchKeepAliveRequestTracker::
    ~SearchPrefetchKeepAliveRequestTracker() {
  base::UmaHistogramEnumeration(
      "Omnibox.SearchPrefetch.KeepAliveRequestFinalStage", current_stage_);
}

void SearchPrefetchKeepAliveRequestTracker::AddStageMetrics(
    const RequestStage& stage) {
  switch (stage.type) {
    case RequestStageType::kLoaderCreated:
      NOTREACHED();

    // Ignore the redirection cases.
    case RequestStageType::kFirstRedirectReceived:
    case RequestStageType::kSecondRedirectReceived:
    case RequestStageType::kThirdOrLaterRedirectReceived:
      break;

    // Even if browser loader is disconnected from the renderer, it should keep
    // fetching, so ignore it.
    case RequestStageType::kLoaderDisconnectedFromRenderer:
      break;

    // Used by kFetchLater type only.
    case RequestStageType::kRequestCancelledByRenderer:
      break;

    case RequestStageType::kResponseReceived:
    case RequestStageType::kRequestFailed:
    case RequestStageType::kRequestStarted:
    case RequestStageType::kRequestCancelledAfterTimeLimit:
    case RequestStageType::kLoaderCompleted:
    case RequestStageType::kRequestRetried:
      current_stage_ = stage.type;
      break;
    case RequestStageType::kBrowserShutdown:
      if (current_stage_ == RequestStageType::kResponseReceived ||
          current_stage_ == RequestStageType::kLoaderCompleted) {
        break;
      }
      current_stage_ = RequestStageType::kBrowserShutdown;
      break;
  }
}

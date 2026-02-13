// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_KEEP_ALIVE_REQUEST_TRACKER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_KEEP_ALIVE_REQUEST_TRACKER_H_

#include "content/public/browser/keep_alive_request_tracker.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
struct ResourceRequest;
}  // namespace network

// Tracks the keepalive requests that are used for updating search prefetch
// states.
// TODO(https://crbug.com/394213503): Move this class to search_preload/ after
// DSE2 is ready to launch.
class SearchPrefetchKeepAliveRequestTracker
    : public content::KeepAliveRequestTracker {
 public:
  static std::unique_ptr<SearchPrefetchKeepAliveRequestTracker>
  MaybeCreateKeepAliveRequestTracker(const network::ResourceRequest& request,
                                     content::BrowserContext* browser_context);

  ~SearchPrefetchKeepAliveRequestTracker() override;

 protected:
  void AddStageMetrics(const RequestStage& stage) override;

 private:
  explicit SearchPrefetchKeepAliveRequestTracker(
      const network::ResourceRequest& request);

  // Track the final stage of a keep alive request.
  RequestStageType current_stage_ = RequestStageType::kLoaderCreated;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_KEEP_ALIVE_REQUEST_TRACKER_H_

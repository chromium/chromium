// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_H_
#define CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/global_routing_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class BrowserContext;
}  // namespace content

// `FromGWSNavigationAndKeepAliveRequestTracker` is a per browser context
// service that tracks navigations and fetch keepalive requests made from Google
// search result pages (SRP).
class FromGWSNavigationAndKeepAliveRequestTracker : public KeyedService {
 public:
  // DO NOT use this constructor directly. Use
  // `FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetTracker` instead.
  //
  // `browser_context` is the browser context that owns `this` via
  // `KeyedService`.
  explicit FromGWSNavigationAndKeepAliveRequestTracker(
      content::BrowserContext* browser_context);
  ~FromGWSNavigationAndKeepAliveRequestTracker() override;

  // Not copyable or movable.
  FromGWSNavigationAndKeepAliveRequestTracker(
      const FromGWSNavigationAndKeepAliveRequestTracker&) = delete;
  FromGWSNavigationAndKeepAliveRequestTracker& operator=(
      const FromGWSNavigationAndKeepAliveRequestTracker&) = delete;

  // Tracks a navigation.
  //
  // `initator_rfh_id` is the ID of the RenderFrameHost that initiates the
  // navigation.
  // `category_id` is the category ID extracted from the navigation URL.
  // `ukm_source_id` is the UKM source ID of the page that initiates the
  // navigation.
  // `navigation_id` is the ID of the navigation.
  virtual void TrackNavigation(content::GlobalRenderFrameHostId initator_rfh_id,
                               int64_t category_id,
                               ukm::SourceId ukm_source_id,
                               int64_t navigation_id);

  // Tracks a keepalive request.
  //
  // `initator_rfh_id` is the ID of the RenderFrameHost that initiates the
  // request.
  // `category_id` is the category ID extracted from the request URL.
  // `ukm_source_id` is the UKM source ID of the page that initiates the
  // request.
  // `keepalive_token` is the token of the fetch keepalive request.
  virtual void TrackKeepAliveRequest(
      content::GlobalRenderFrameHostId initator_rfh_id,
      int64_t category_id,
      ukm::SourceId ukm_source_id,
      base::UnguessableToken keepalive_token);

 private:
  // Represents a category ID within a RenderFrameHost.
  struct FrameCategory {
    content::GlobalRenderFrameHostId initator_rfh_id;
    int64_t category_id;
    ukm::SourceId ukm_source_id;

    bool operator<(const FrameCategory& rhs) const;
    bool operator==(const FrameCategory& rhs) const;
  };

  // Represents a navigation and a keepalive request made from the same page.
  // Note that both of them must have the same category ID in their URLs.
  struct NavigationAndKeepAliveRequest {
    std::optional<int64_t> navigation_id = std::nullopt;
    std::optional<base::UnguessableToken> keepalive_token = std::nullopt;
  };

  void LogUkmEvent(const FrameCategory& frame_category,
                   const NavigationAndKeepAliveRequest& request);

  // Starts `cleanup_timer_` if it's not running yet.
  void StartCleanupTimer();
  // A callback to clean up unused mappings.
  void OnCleanupTimeout();

  // The `BrowserContext` that owns `this` via `KeyedService`.
  raw_ptr<content::BrowserContext> browser_context_;

  // A map from a `FrameCategory` to a list of `NavigationAndKeepAliveRequest`s
  // made from the same page.
  std::map<FrameCategory, std::vector<NavigationAndKeepAliveRequest>>
      per_frame_category_navigation_and_keepalive_requests_;

  // A timer to clean up `per_frame_category_navigation_and_keepalive_requests_`
  // periodically.
  base::RepeatingTimer cleanup_timer_;

  // Must be the last field.
  base::WeakPtrFactory<FromGWSNavigationAndKeepAliveRequestTracker>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LOADER_FROM_GWS_NAVIGATION_AND_KEEP_ALIVE_REQUEST_TRACKER_H_

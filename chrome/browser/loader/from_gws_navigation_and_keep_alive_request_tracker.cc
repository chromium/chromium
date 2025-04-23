// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/features.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

BASE_FEATURE_PARAM(base::TimeDelta,
                   kBeaconLeakageLoggingCleanupTimeout,
                   &page_load_metrics::features::kBeaconLeakageLogging,
                   "cleanup_timeout",
                   base::Seconds(180));
}  // namespace

FromGWSNavigationAndKeepAliveRequestTracker::
    FromGWSNavigationAndKeepAliveRequestTracker(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

FromGWSNavigationAndKeepAliveRequestTracker::
    ~FromGWSNavigationAndKeepAliveRequestTracker() = default;

void FromGWSNavigationAndKeepAliveRequestTracker::TrackNavigation(
    content::GlobalRenderFrameHostId initator_rfh_id,
    int64_t category_id,
    ukm::SourceId ukm_source_id,
    int64_t navigation_id) {
  FrameCategory frame_category{
      initator_rfh_id,
      category_id,
      ukm_source_id,
  };

  if (per_frame_category_navigation_and_keepalive_requests_.find(
          frame_category) ==
      per_frame_category_navigation_and_keepalive_requests_.end()) {
    per_frame_category_navigation_and_keepalive_requests_[frame_category] = {};
  } else {
    auto& requests =
        per_frame_category_navigation_and_keepalive_requests_[frame_category];
    for (size_t i = 0; i < requests.size(); ++i) {
      if (requests[i].keepalive_token.has_value() &&
          !requests[i].navigation_id.has_value()) {
        // Found a keepalive request token without a navigation ID.
        // Logs a UKM event and removes the request from the list.
        requests[i].navigation_id = navigation_id;
        LogUkmEvent(frame_category, requests[i]);
        requests.erase(requests.begin() + i);
        return;
      }
    }
    // No keepalive request token without a navigation ID.
  }

  per_frame_category_navigation_and_keepalive_requests_[frame_category]
      .push_back({.navigation_id = navigation_id});
  StartCleanupTimer();
}

void FromGWSNavigationAndKeepAliveRequestTracker::TrackKeepAliveRequest(
    content::GlobalRenderFrameHostId initator_rfh_id,
    int64_t category_id,
    ukm::SourceId ukm_source_id,
    base::UnguessableToken keepalive_token) {
  FrameCategory frame_category{
      initator_rfh_id,
      category_id,
      ukm_source_id,
  };

  if (per_frame_category_navigation_and_keepalive_requests_.find(
          frame_category) ==
      per_frame_category_navigation_and_keepalive_requests_.end()) {
    per_frame_category_navigation_and_keepalive_requests_[frame_category] = {};
  } else {
    auto& requests =
        per_frame_category_navigation_and_keepalive_requests_[frame_category];
    for (size_t i = 0; i < requests.size(); ++i) {
      if (!requests[i].keepalive_token.has_value() &&
          requests[i].navigation_id.has_value()) {
        // Found a navigation without a keepalive request token.
        // Logs a UKM event and removes the request from the list.
        requests[i].keepalive_token = keepalive_token;
        LogUkmEvent(frame_category, requests[i]);
        requests.erase(requests.begin() + i);
        return;
      }
    }
  }

  per_frame_category_navigation_and_keepalive_requests_[frame_category]
      .push_back({.keepalive_token = keepalive_token});
  StartCleanupTimer();
}

void FromGWSNavigationAndKeepAliveRequestTracker::LogUkmEvent(
    const FrameCategory& frame_category,
    const NavigationAndKeepAliveRequest& request) {
  CHECK(request.navigation_id.has_value());
  CHECK(request.keepalive_token.has_value());

  ukm::builders::FetchKeepAliveRequest_WithCategory_Navigation ukm_builder(
      frame_category.ukm_source_id);

  ukm_builder.SetCategory(frame_category.category_id);
  ukm_builder.SetNavigationId(*request.navigation_id);
  ukm_builder.SetId_Low(request.keepalive_token->GetLowForSerialization());
  ukm_builder.SetId_High(request.keepalive_token->GetHighForSerialization());

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_builder.Record(ukm_recorder->Get());
}

void FromGWSNavigationAndKeepAliveRequestTracker::StartCleanupTimer() {
  if (cleanup_timer_.IsRunning()) {
    return;
  }

  cleanup_timer_.Start(
      FROM_HERE, kBeaconLeakageLoggingCleanupTimeout.Get(),
      base::BindRepeating(
          &FromGWSNavigationAndKeepAliveRequestTracker::OnCleanupTimeout,
          weak_ptr_factory_.GetWeakPtr()));
}

void FromGWSNavigationAndKeepAliveRequestTracker::OnCleanupTimeout() {
  per_frame_category_navigation_and_keepalive_requests_.clear();
}

bool FromGWSNavigationAndKeepAliveRequestTracker::FrameCategory::operator<(
    const FrameCategory& rhs) const {
  return std::tie(initator_rfh_id, category_id, ukm_source_id) <
         std::tie(rhs.initator_rfh_id, rhs.category_id, rhs.ukm_source_id);
}

bool FromGWSNavigationAndKeepAliveRequestTracker::FrameCategory::operator==(
    const FrameCategory& rhs) const {
  return std::tie(initator_rfh_id, category_id, ukm_source_id) ==
         std::tie(rhs.initator_rfh_id, rhs.category_id, rhs.ukm_source_id);
}

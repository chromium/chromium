// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_TRACKER_H_
#define CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_TRACKER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

// ChromeKeepAliveRequestTracker is responsible for recording browser-side
// metrics for an eligible fetch keepalive request.
//
// The UKM event logging happens only once when this tracker is about to be
// destroyed.
//
// See
// https://docs.google.com/document/d/1byKFqqKTsVFnj6rSb7ZjHi4LuRi1LNb-vf4maQbmSSQ/edit?resourcekey=0-7GAe1ae8j_55DMcvp3U3IQ&tab=t.0#heading=h.bk553qrz82t0
class ChromeKeepAliveRequestTracker : public content::KeepAliveRequestTracker {
 public:
  ~ChromeKeepAliveRequestTracker() override;

  // Returns a tracker instance if `request` is eligible to be tracked.
  //
  // `ukm_source_id` is the UKM ID to associate with the events logged by the
  // returned tracker.
  // `is_context_detached_callback` tells if the context of `request` is
  // detached at the time running the callback.
  static std::unique_ptr<ChromeKeepAliveRequestTracker>
  MaybeCreateKeepAliveRequestTracker(
      const network::ResourceRequest& request,
      std::optional<ukm::SourceId> ukm_source_id,
      IsContextDetachedCallback is_context_detached_callback);

 private:
  ChromeKeepAliveRequestTracker(
      RequestType request_type,
      uint32_t request_category,
      ukm::SourceId ukm_source_id,
      IsContextDetachedCallback is_context_detached_callback,
      base::UnguessableToken keepalive_token);

  void AddStageMetrics(const RequestStage& stage) override;

  // Logs a UKM event for the tracked request.
  void LogUkmEvent();

  // Records the time when this tracker is created.
  const base::TimeTicks created_time_;

  // The UKM builder for the tracked request.
  ukm::builders::FetchKeepAliveRequest_WithCategory ukm_builder_;

  // A callback to tell if the context of the request is detached.
  IsContextDetachedCallback is_context_detached_callback_;
};

#endif  // CHROME_BROWSER_LOADER_KEEP_ALIVE_REQUEST_TRACKER_H_

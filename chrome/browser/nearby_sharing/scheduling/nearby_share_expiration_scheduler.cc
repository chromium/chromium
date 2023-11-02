// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_expiration_scheduler.h"

#include <utility>

NearbyShareExpirationScheduler::NearbyShareExpirationScheduler(
    ExpirationTimeFunctor expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    OnRequestCallback on_request_callback,
    const base::Clock* clock)
    : NearbyShareSchedulerBase(retry_failures,
                               require_connectivity,
                               pref_name,
                               pref_service,
                               std::move(on_request_callback),
                               clock),
      expiration_time_functor_(std::move(expiration_time_functor)) {}

NearbyShareExpirationScheduler::~NearbyShareExpirationScheduler() = default;

absl::optional<base::TimeDelta>
NearbyShareExpirationScheduler::TimeUntilRecurringRequest(
    base::Time now) const {
  absl::optional<base::Time> expiration_time = expiration_time_functor_.Run();
  if (!expiration_time)
    return absl::nullopt;

  if (*expiration_time <= now)
    return base::Seconds(0);

  return *expiration_time - now;
}

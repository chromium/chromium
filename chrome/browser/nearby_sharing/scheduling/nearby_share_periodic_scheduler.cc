// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_periodic_scheduler.h"

#include <algorithm>
#include <utility>

NearbySharePeriodicScheduler::NearbySharePeriodicScheduler(
    base::TimeDelta request_period,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    OnRequestCallback callback,
    const base::Clock* clock)
    : NearbyShareSchedulerBase(retry_failures,
                               require_connectivity,
                               pref_name,
                               pref_service,
                               std::move(callback),
                               clock),
      request_period_(request_period) {}

NearbySharePeriodicScheduler::~NearbySharePeriodicScheduler() = default;

absl::optional<base::TimeDelta>
NearbySharePeriodicScheduler::TimeUntilRecurringRequest(base::Time now) const {
  absl::optional<base::Time> last_success_time = GetLastSuccessTime();

  // Immediately run a first-time request.
  if (!last_success_time)
    return base::Seconds(0);

  base::TimeDelta time_elapsed_since_last_success = now - *last_success_time;

  return std::max(base::Seconds(0),
                  request_period_ - time_elapsed_since_last_success);
}

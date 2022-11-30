// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_on_demand_scheduler.h"

#include <utility>

NearbyShareOnDemandScheduler::NearbyShareOnDemandScheduler(
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
                               clock) {}

NearbyShareOnDemandScheduler::~NearbyShareOnDemandScheduler() = default;

absl::optional<base::TimeDelta>
NearbyShareOnDemandScheduler::TimeUntilRecurringRequest(base::Time now) const {
  return absl::nullopt;
}

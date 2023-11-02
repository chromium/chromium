// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A NearbyShareSchedulerBase that does not schedule recurring tasks.
class NearbyShareOnDemandScheduler : public NearbyShareSchedulerBase {
 public:
  // See NearbyShareSchedulerBase for a description of inputs.
  NearbyShareOnDemandScheduler(bool retry_failures,
                               bool require_connectivity,
                               const std::string& pref_name,
                               PrefService* pref_service,
                               OnRequestCallback callback,
                               const base::Clock* clock);

  ~NearbyShareOnDemandScheduler() override;

 private:
  // Return absl::nullopt so as not to schedule recurring requests.
  absl::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_PERIODIC_SCHEDULER_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_PERIODIC_SCHEDULER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A NearbyShareSchedulerBase that schedules periodic tasks at fixed intervals.
// Immediate requests and/or failure retries can interrupt this pattern. The
// periodic taks is always updated to run a fixed delay after the last
// successful request.
class NearbySharePeriodicScheduler : public NearbyShareSchedulerBase {
 public:
  // |request_period|: The fixed delay between periodic requests.
  // See NearbyShareSchedulerBase for a description of other inputs.
  NearbySharePeriodicScheduler(base::TimeDelta request_period,
                               bool retry_failures,
                               bool require_connectivity,
                               const std::string& pref_name,
                               PrefService* pref_service,
                               OnRequestCallback callback,
                               const base::Clock* clock);

  ~NearbySharePeriodicScheduler() override;

 private:
  // Returns the time until the next periodic request using the time since
  // the last success. Immediately runs a first-time periodic request.
  absl::optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;

  base::TimeDelta request_period_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_PERIODIC_SCHEDULER_H_

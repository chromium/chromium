// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_base.h"

// A NearbyShareSchedulerBase that schedules recurring tasks based on an
// expiration time provided by the owner.
class NearbyShareExpirationScheduler : public NearbyShareSchedulerBase {
 public:
  using ExpirationTimeFunctor =
      base::RepeatingCallback<base::Optional<base::Time>()>;

  // |expiration_time_functor|: A function provided by the owner that returns
  //     the next expiration time.
  // See NearbyShareSchedulerBase for a description of other inputs.
  NearbyShareExpirationScheduler(ExpirationTimeFunctor expiration_time_functor,
                                 bool retry_failures,
                                 bool require_connectivity,
                                 const std::string& pref_name,
                                 PrefService* pref_service,
                                 OnRequestCallback on_request_callback,
                                 const base::Clock* clock);

  ~NearbyShareExpirationScheduler() override;

 protected:
  base::Optional<base::TimeDelta> TimeUntilRecurringRequest(
      base::Time now) const override;

  ExpirationTimeFunctor expiration_time_functor_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_

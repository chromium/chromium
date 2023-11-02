// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_

#include <memory>
#include <string>

#include "base/time/default_clock.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"

class NearbyShareScheduler;
class PrefService;

// Used to create instances of NearbyShareExpirationScheduler,
// NearbyShareOnDemandScheduler, and NearbySharePeriodicScheduler. A fake
// factory can also be set for testing purposes.
class NearbyShareSchedulerFactory {
 public:
  static std::unique_ptr<NearbyShareScheduler> CreateExpirationScheduler(
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback on_request_callback,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static std::unique_ptr<NearbyShareScheduler> CreateOnDemandScheduler(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static std::unique_ptr<NearbyShareScheduler> CreatePeriodicScheduler(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  static void SetFactoryForTesting(NearbyShareSchedulerFactory* test_factory);

 protected:
  virtual ~NearbyShareSchedulerFactory();

  virtual std::unique_ptr<NearbyShareScheduler>
  CreateExpirationSchedulerInstance(
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback on_request_callback,
      const base::Clock* clock) = 0;

  virtual std::unique_ptr<NearbyShareScheduler> CreateOnDemandSchedulerInstance(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock) = 0;

  virtual std::unique_ptr<NearbyShareScheduler> CreatePeriodicSchedulerInstance(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock) = 0;

 private:
  static NearbyShareSchedulerFactory* test_factory_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_

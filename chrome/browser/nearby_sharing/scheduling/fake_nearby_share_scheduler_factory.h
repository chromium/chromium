// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"

class NearbyShareScheduler;
class PrefService;

// A fake NearbyShareScheduler factory that creates instances of
// FakeNearbyShareScheduler instead of expiration, on-demand, or periodic
// scheduler. It stores the factory input parameters as well as a raw pointer to
// the fake scheduler for each instance created.
class FakeNearbyShareSchedulerFactory : public NearbyShareSchedulerFactory {
 public:
  struct ExpirationInstance {
    ExpirationInstance();
    ExpirationInstance(ExpirationInstance&&);
    ~ExpirationInstance();

    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor;
    bool retry_failures;
    bool require_connectivity;
    PrefService* pref_service = nullptr;
    const base::Clock* clock = nullptr;
  };

  struct OnDemandInstance {
    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    bool retry_failures;
    bool require_connectivity;
    PrefService* pref_service = nullptr;
    const base::Clock* clock = nullptr;
  };

  struct PeriodicInstance {
    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    base::TimeDelta request_period;
    bool retry_failures;
    bool require_connectivity;
    PrefService* pref_service = nullptr;
    const base::Clock* clock = nullptr;
  };

  FakeNearbyShareSchedulerFactory();
  ~FakeNearbyShareSchedulerFactory() override;

  const std::map<std::string, ExpirationInstance>&
  pref_name_to_expiration_instance() const {
    return pref_name_to_expiration_instance_;
  }

  const std::map<std::string, OnDemandInstance>&
  pref_name_to_on_demand_instance() const {
    return pref_name_to_on_demand_instance_;
  }

  const std::map<std::string, PeriodicInstance>&
  pref_name_to_periodic_instance() const {
    return pref_name_to_periodic_instance_;
  }

 private:
  // NearbyShareSchedulerFactory:
  std::unique_ptr<NearbyShareScheduler> CreateExpirationSchedulerInstance(
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback on_request_callback,
      const base::Clock* clock) override;
  std::unique_ptr<NearbyShareScheduler> CreateOnDemandSchedulerInstance(
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock) override;
  std::unique_ptr<NearbyShareScheduler> CreatePeriodicSchedulerInstance(
      base::TimeDelta request_period,
      bool retry_failures,
      bool require_connectivity,
      const std::string& pref_name,
      PrefService* pref_service,
      NearbyShareScheduler::OnRequestCallback callback,
      const base::Clock* clock) override;

  std::map<std::string, ExpirationInstance> pref_name_to_expiration_instance_;
  std::map<std::string, OnDemandInstance> pref_name_to_on_demand_instance_;
  std::map<std::string, PeriodicInstance> pref_name_to_periodic_instance_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_

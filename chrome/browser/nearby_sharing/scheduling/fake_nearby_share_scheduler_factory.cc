// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler_factory.h"

#include <utility>

FakeNearbyShareSchedulerFactory::ExpirationInstance::ExpirationInstance() =
    default;

FakeNearbyShareSchedulerFactory::ExpirationInstance::ExpirationInstance(
    ExpirationInstance&&) = default;

FakeNearbyShareSchedulerFactory::ExpirationInstance::~ExpirationInstance() =
    default;

FakeNearbyShareSchedulerFactory::FakeNearbyShareSchedulerFactory() = default;

FakeNearbyShareSchedulerFactory::~FakeNearbyShareSchedulerFactory() = default;

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreateExpirationSchedulerInstance(
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback on_request_callback,
    const base::Clock* clock) {
  ExpirationInstance instance;
  instance.expiration_time_functor = std::move(expiration_time_functor);
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.clock = clock;

  auto scheduler = std::make_unique<FakeNearbyShareScheduler>(
      std::move(on_request_callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_expiration_instance_.erase(pref_name);
  pref_name_to_expiration_instance_.emplace(pref_name, std::move(instance));

  return scheduler;
}

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreateOnDemandSchedulerInstance(
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback callback,
    const base::Clock* clock) {
  OnDemandInstance instance;
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.clock = clock;

  auto scheduler =
      std::make_unique<FakeNearbyShareScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_on_demand_instance_.erase(pref_name);
  pref_name_to_on_demand_instance_.emplace(pref_name, instance);

  return scheduler;
}

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreatePeriodicSchedulerInstance(
    base::TimeDelta request_period,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback callback,
    const base::Clock* clock) {
  PeriodicInstance instance;
  instance.request_period = request_period;
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;
  instance.pref_service = pref_service;
  instance.clock = clock;

  auto scheduler =
      std::make_unique<FakeNearbyShareScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_periodic_instance_.erase(pref_name);
  pref_name_to_periodic_instance_.emplace(pref_name, instance);

  return scheduler;
}

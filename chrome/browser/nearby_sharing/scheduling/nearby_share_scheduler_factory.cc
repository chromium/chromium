// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_factory.h"

#include <utility>

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_on_demand_scheduler.h"
#include "chrome/browser/nearby_sharing/scheduling/nearby_share_periodic_scheduler.h"

// static
NearbyShareSchedulerFactory* NearbyShareSchedulerFactory::test_factory_ =
    nullptr;

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreateExpirationScheduler(
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback on_request_callback,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateExpirationSchedulerInstance(
        std::move(expiration_time_functor), retry_failures,
        require_connectivity, pref_name, pref_service,
        std::move(on_request_callback), clock);
  }

  return std::make_unique<NearbyShareExpirationScheduler>(
      std::move(expiration_time_functor), retry_failures, require_connectivity,
      pref_name, pref_service, std::move(on_request_callback), clock);
}

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreateOnDemandScheduler(
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback callback,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateOnDemandSchedulerInstance(
        retry_failures, require_connectivity, pref_name, pref_service,
        std::move(callback), clock);
  }

  return std::make_unique<NearbyShareOnDemandScheduler>(
      retry_failures, require_connectivity, pref_name, pref_service,
      std::move(callback), clock);
}

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreatePeriodicScheduler(
    base::TimeDelta request_period,
    bool retry_failures,
    bool require_connectivity,
    const std::string& pref_name,
    PrefService* pref_service,
    NearbyShareScheduler::OnRequestCallback callback,
    const base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreatePeriodicSchedulerInstance(
        request_period, retry_failures, require_connectivity, pref_name,
        pref_service, std::move(callback), clock);
  }

  return std::make_unique<NearbySharePeriodicScheduler>(
      request_period, retry_failures, require_connectivity, pref_name,
      pref_service, std::move(callback), clock);
}

// static
void NearbyShareSchedulerFactory::SetFactoryForTesting(
    NearbyShareSchedulerFactory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareSchedulerFactory::~NearbyShareSchedulerFactory() = default;

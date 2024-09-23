// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"

#include "base/time/clock.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

base::Clock* OfflineSigninLimiterFactory::clock_for_testing_ = nullptr;

// static
OfflineSigninLimiterFactory* OfflineSigninLimiterFactory::GetInstance() {
  static base::NoDestructor<OfflineSigninLimiterFactory> instance;
  return instance.get();
}

// static
OfflineSigninLimiter* OfflineSigninLimiterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OfflineSigninLimiter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
void OfflineSigninLimiterFactory::SetClockForTesting(base::Clock* clock) {
  clock_for_testing_ = clock;
}

OfflineSigninLimiterFactory::OfflineSigninLimiterFactory()
    : ProfileKeyedServiceFactory(
          "OfflineSigninLimiter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

OfflineSigninLimiterFactory::~OfflineSigninLimiterFactory() = default;

std::unique_ptr<KeyedService>
OfflineSigninLimiterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OfflineSigninLimiter>(static_cast<Profile*>(context),
                                                clock_for_testing_);
}

}  // namespace ash

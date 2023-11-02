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
  return base::Singleton<OfflineSigninLimiterFactory>::get();
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
    : ProfileKeyedServiceFactory("OfflineSigninLimiter") {}

OfflineSigninLimiterFactory::~OfflineSigninLimiterFactory() {}

KeyedService* OfflineSigninLimiterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OfflineSigninLimiter(static_cast<Profile*>(context),
                                  clock_for_testing_);
}

}  // namespace ash

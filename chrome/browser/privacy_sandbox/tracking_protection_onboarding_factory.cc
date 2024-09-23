// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

TrackingProtectionOnboardingFactory*
TrackingProtectionOnboardingFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionOnboardingFactory> instance;
  return instance.get();
}

privacy_sandbox::TrackingProtectionOnboarding*
TrackingProtectionOnboardingFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::TrackingProtectionOnboarding*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TrackingProtectionOnboardingFactory::TrackingProtectionOnboardingFactory()
    : ProfileKeyedServiceFactory("TrackingProtectionOnboarding",
                                 ProfileSelections::Builder()
                                     // Excluding Ash Internal profiles such as
                                     // the signin or the lockscreen profile.
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {}

std::unique_ptr<KeyedService>
TrackingProtectionOnboardingFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return std::make_unique<privacy_sandbox::TrackingProtectionOnboarding>(
      profile->GetPrefs());
}

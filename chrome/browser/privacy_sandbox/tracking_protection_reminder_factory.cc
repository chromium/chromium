// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

TrackingProtectionReminderFactory*
TrackingProtectionReminderFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionReminderFactory> instance;
  return instance.get();
}

privacy_sandbox::TrackingProtectionReminderService*
TrackingProtectionReminderFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::TrackingProtectionReminderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TrackingProtectionReminderFactory::TrackingProtectionReminderFactory()
    : ProfileKeyedServiceFactory("TrackingProtectionReminder",
                                 // Exclude Ash login and lockscreen.
                                 ProfileSelections::Builder()
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {
  DependsOn(TrackingProtectionOnboardingFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TrackingProtectionReminderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::TrackingProtectionReminderService>(
      profile->GetPrefs(),
      TrackingProtectionOnboardingFactory::GetForProfile(profile));
}

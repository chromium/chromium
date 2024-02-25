// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

TrackingProtectionSettingsFactory*
TrackingProtectionSettingsFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionSettingsFactory> instance;
  return instance.get();
}

privacy_sandbox::TrackingProtectionSettings*
TrackingProtectionSettingsFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::TrackingProtectionSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TrackingProtectionSettingsFactory::TrackingProtectionSettingsFactory()
    : ProfileKeyedServiceFactory(
          "TrackingProtectionSettings",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): If `WithGuest` changes for
              // CookieControlsServiceFactory or PrivacySandboxServiceFactory
              // it should also be reflected here.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(TrackingProtectionOnboardingFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TrackingProtectionSettingsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (profiles::IsRegularUserProfile(profile)) {
    if (profile->GetPrefs()->GetBoolean(
            prefs::kTrackingProtection3pcdEnabled)) {
      base::UmaHistogramBoolean("Settings.TrackingProtection.Enabled", true);
      base::UmaHistogramBoolean(
          "Settings.TrackingProtection.BlockAllThirdParty",
          profile->GetPrefs()->GetBoolean(prefs::kBlockAll3pcToggleEnabled));
    } else {
      base::UmaHistogramBoolean("Settings.TrackingProtection.Enabled", false);
    }
    if (base::FeatureList::IsEnabled(privacy_sandbox::kIpProtectionV1)) {
      base::UmaHistogramBoolean(
          "Settings.IpProtection.Enabled",
          profile->GetPrefs()->GetBoolean(prefs::kIpProtectionEnabled));
    }
  }

  return std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
      profile->GetPrefs(),
      TrackingProtectionOnboardingFactory::GetForProfile(profile),
      profile->IsIncognitoProfile());
}

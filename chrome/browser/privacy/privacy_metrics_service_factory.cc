// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy/privacy_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

PrivacyMetricsServiceFactory* PrivacyMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacyMetricsServiceFactory> instance;
  return instance.get();
}

PrivacyMetricsService* PrivacyMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacyMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacyMetricsServiceFactory::PrivacyMetricsServiceFactory()
    // No metrics recorded for OTR profiles, system profiles, guest
    // profiles, or unusual ChromeOS profiles.
    : ProfileKeyedServiceFactory(
          "PrivacyMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* PrivacyMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PrivacyMetricsService(
      profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#endif

PrivacySandboxServiceFactory* PrivacySandboxServiceFactory::GetInstance() {
  return base::Singleton<PrivacySandboxServiceFactory>::get();
}

PrivacySandboxService* PrivacySandboxServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacySandboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxServiceFactory::PrivacySandboxServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxService",
          // TODO(crbug.com/1284295): Determine whether this actually needs to
          // be created, or whether all usage in OTR contexts can be removed.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(browsing_topics::BrowsingTopicsServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(TrustSafetySentimentServiceFactory::GetInstance());
#endif
  DependsOn(
      first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance());
}

KeyedService* PrivacySandboxServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PrivacySandboxService(
      PrivacySandboxSettingsFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile).get(), profile->GetPrefs(),
      profile->GetDefaultStoragePartition()->GetInterestGroupManager(),
      profile_metrics::GetBrowserProfileType(profile),
      (!profile->IsGuestSession() || profile->IsOffTheRecord())
          ? profile->GetBrowsingDataRemover()
          : nullptr,
      HostContentSettingsMapFactory::GetForProfile(profile),
#if !BUILDFLAG(IS_ANDROID)
      TrustSafetySentimentServiceFactory::GetForProfile(profile),
#endif
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile),
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(context));
}

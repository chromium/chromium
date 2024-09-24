// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_countries_impl.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/eligibility_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/storage_partition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace {

profile_metrics::BrowserProfileType GetProfileType(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Alias the "system" profiles which present as regular profiles for metrics
  // purposes (e.g. signin screen), to system metrics profiles. This is done
  // here as, due to dependency injection, the service itself does not hold a
  // profile pointer.
  // TODO (crbug.com/1450490) - Move to simply not creating the service for
  // these types of profiles.
  if (!ash::IsUserBrowserContext(profile)) {
    return profile_metrics::BrowserProfileType::kSystem;
  }
#endif
  return profile_metrics::GetBrowserProfileType(profile);
}

}  // namespace

PrivacySandboxServiceFactory* PrivacySandboxServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxServiceFactory> instance;
  return instance.get();
}

PrivacySandboxService* PrivacySandboxServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacySandboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// LINT.IfChange(PrivacySandboxService)
PrivacySandboxServiceFactory::PrivacySandboxServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxService",
          // TODO(crbug.com/40814288): Determine whether this actually needs to
          // be created, or whether all usage in OTR contexts can be removed.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(browsing_topics::BrowsingTopicsServiceFactory::GetInstance());
  DependsOn(TrackingProtectionSettingsFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(TrustSafetySentimentServiceFactory::GetInstance());
#endif
  DependsOn(
      first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance());

  // The Eligibility service should be created before the Privacy Sandbox
  // service is created to determine the cookie deprecation experiment
  // eligibility.
  DependsOn(tpcd::experiment::EligibilityServiceFactory::GetInstance());
}
// LINT.ThenChange(/chrome/browser/privacy_sandbox/privacy_sandbox_notice_service_factory.cc:PrivacySandboxNoticeService)

std::unique_ptr<KeyedService>
PrivacySandboxServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  static PrivacySandboxCountriesImpl countries_instance;
  return std::make_unique<PrivacySandboxServiceImpl>(
      PrivacySandboxSettingsFactory::GetForProfile(profile),
      TrackingProtectionSettingsFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile), profile->GetPrefs(),
      profile->GetDefaultStoragePartition()->GetInterestGroupManager(),
      GetProfileType(profile),
      (!profile->IsGuestSession() || profile->IsOffTheRecord())
          ? profile->GetBrowsingDataRemover()
          : nullptr,
      HostContentSettingsMapFactory::GetForProfile(profile),
#if !BUILDFLAG(IS_ANDROID)
      TrustSafetySentimentServiceFactory::GetForProfile(profile),
#endif
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile),
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(context),
      &countries_instance);
}

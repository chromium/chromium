// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#endif

ProfileNetworkContextService*
ProfileNetworkContextServiceFactory::GetForContext(
    content::BrowserContext* browser_context) {
  return static_cast<ProfileNetworkContextService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

ProfileNetworkContextServiceFactory*
ProfileNetworkContextServiceFactory::GetInstance() {
  return base::Singleton<ProfileNetworkContextServiceFactory>::get();
}

ProfileNetworkContextServiceFactory::ProfileNetworkContextServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProfileNetworkContextService",
          // Create separate service for incognito profiles.
          ProfileSelections::BuildForRegularAndIncognito()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(chromeos::CertificateProviderServiceFactory::GetInstance());
#endif
  DependsOn(PrivacySandboxSettingsFactory::GetInstance());
  DependsOn(
      first_party_sets::FirstPartySetsPolicyServiceFactory::GetInstance());
}

ProfileNetworkContextServiceFactory::~ProfileNetworkContextServiceFactory() {}

KeyedService* ProfileNetworkContextServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ProfileNetworkContextService(Profile::FromBrowserContext(profile));
}

bool ProfileNetworkContextServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

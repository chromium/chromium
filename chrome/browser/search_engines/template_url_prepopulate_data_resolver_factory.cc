// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_prepopulate_data_resolver_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"

namespace TemplateURLPrepopulateData {

// static
Resolver* ResolverFactory::GetForProfile(Profile* profile) {
  return static_cast<Resolver*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ResolverFactory* ResolverFactory::GetInstance() {
  return base::Singleton<ResolverFactory>::get();
}

ResolverFactory::ResolverFactory()
    : ProfileKeyedServiceFactory(
          "TemplateURLPrepopulateDataResolver",
          // Service intended as a helper / forwarder to other services. So it
          // should be as available as possible, other services are responsible
          // of forwarding to the parent profile as they see fit.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kOwnInstance)
              // Would crash (`CHECK_DEREF`) if reached from a system profile,
              // as it would not have access to some services it depends on.
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(
      regional_capabilities::RegionalCapabilitiesServiceFactory::GetInstance());
}
ResolverFactory::~ResolverFactory() = default;

std::unique_ptr<KeyedService>
ResolverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  return std::make_unique<Resolver>(
      CHECK_DEREF(profile->GetPrefs()),
      CHECK_DEREF(regional_capabilities::RegionalCapabilitiesServiceFactory::
                      GetForProfile(profile)));
}

}  // namespace TemplateURLPrepopulateData

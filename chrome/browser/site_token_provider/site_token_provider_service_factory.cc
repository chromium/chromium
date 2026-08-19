// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace site_token_provider {

// static
SiteTokenProviderService* SiteTokenProviderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SiteTokenProviderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SiteTokenProviderServiceFactory*
SiteTokenProviderServiceFactory::GetInstance() {
  static base::NoDestructor<SiteTokenProviderServiceFactory> instance;
  return instance.get();
}

SiteTokenProviderServiceFactory::SiteTokenProviderServiceFactory()
    : ProfileKeyedServiceFactory(
          "SiteTokenProviderService",
          ProfileSelections::Builder()
              // Create the service for regular profiles, but explicitly
              // disable it for off-the-record (incognito) profiles.
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Disabled in guest profiles.
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SiteTokenProviderServiceFactory::~SiteTokenProviderServiceFactory() = default;

std::unique_ptr<KeyedService>
SiteTokenProviderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSiteTokenProviderEnabled)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  content::StoragePartition* storage_partition =
      profile->GetDefaultStoragePartition();

  auto provider = SiteTokenProvider::Create(
      IdentityManagerFactory::GetForProfile(profile),
      storage_partition->GetURLLoaderFactoryForBrowserProcess());

  return std::make_unique<SiteTokenProviderService>(
      IdentityManagerFactory::GetForProfile(profile), std::move(provider));
}

bool SiteTokenProviderServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace site_token_provider

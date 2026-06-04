// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_cookie_provider/site_cookie_provider_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/site_cookie_provider/features.h"
#include "components/site_cookie_provider/site_cookie_provider.h"
#include "components/site_cookie_provider/site_cookie_provider_service.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace site_cookie_provider {

// static
SiteCookieProviderService* SiteCookieProviderServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SiteCookieProviderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SiteCookieProviderServiceFactory*
SiteCookieProviderServiceFactory::GetInstance() {
  static base::NoDestructor<SiteCookieProviderServiceFactory> instance;
  return instance.get();
}

SiteCookieProviderServiceFactory::SiteCookieProviderServiceFactory()
    : ProfileKeyedServiceFactory(
          "SiteCookieProviderService",
          ProfileSelections::Builder()
              // Create the service for regular profiles, but explicitly
              // disable it for off-the-record (incognito) profiles.
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Disabled in guest profiles.
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SiteCookieProviderServiceFactory::~SiteCookieProviderServiceFactory() = default;

std::unique_ptr<KeyedService>
SiteCookieProviderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kSiteCookieProviderEnabled)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  content::StoragePartition* storage_partition =
      profile->GetDefaultStoragePartition();

  mojo::PendingRemote<network::mojom::CookieManager> cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      cookie_manager.InitWithNewPipeAndPassReceiver());

  auto provider = SiteCookieProvider::Create(
      IdentityManagerFactory::GetForProfile(profile), std::move(cookie_manager),
      storage_partition->GetURLLoaderFactoryForBrowserProcess());

  return std::make_unique<SiteCookieProviderService>(
      IdentityManagerFactory::GetForProfile(profile), std::move(provider));
}

bool SiteCookieProviderServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace site_cookie_provider

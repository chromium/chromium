// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/net/enterprise_network_auth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/enterprise/net/core/features.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
enterprise_net::EnterpriseProxyService*
EnterpriseProxyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<enterprise_net::EnterpriseProxyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EnterpriseProxyServiceFactory* EnterpriseProxyServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseProxyServiceFactory> instance;
  return instance.get();
}

EnterpriseProxyServiceFactory::EnterpriseProxyServiceFactory()
    : ProfileKeyedServiceFactory(
          "EnterpriseProxyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(EnterpriseNetworkAuthServiceFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
}

EnterpriseProxyServiceFactory::~EnterpriseProxyServiceFactory() = default;

std::unique_ptr<KeyedService>
EnterpriseProxyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto url_loader_factory_callback = base::BindRepeating(
      [](Profile* profile) {
        return profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess();
      },
      profile);
  return std::make_unique<enterprise_net::EnterpriseProxyService>(
      profile->GetPrefs(),
      EnterpriseNetworkAuthServiceFactory::GetForProfile(profile),
      std::move(url_loader_factory_callback),
      enterprise::ProfileIdServiceFactory::GetForProfile(profile));
}

bool EnterpriseProxyServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

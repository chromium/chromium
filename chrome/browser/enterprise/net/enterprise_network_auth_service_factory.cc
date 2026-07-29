// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_network_auth_service_factory.h"

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/features.h"

// static
enterprise_net::EnterpriseNetworkAuthService*
EnterpriseNetworkAuthServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<enterprise_net::EnterpriseNetworkAuthService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EnterpriseNetworkAuthServiceFactory*
EnterpriseNetworkAuthServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseNetworkAuthServiceFactory> instance;
  return instance.get();
}

EnterpriseNetworkAuthServiceFactory::EnterpriseNetworkAuthServiceFactory()
    : ProfileKeyedServiceFactory("EnterpriseNetworkAuthService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
}

EnterpriseNetworkAuthServiceFactory::~EnterpriseNetworkAuthServiceFactory() =
    default;

bool EnterpriseNetworkAuthServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
EnterpriseNetworkAuthServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<enterprise_net::EnterpriseNetworkAuthService>(
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      enterprise::ProfileIdServiceFactory::GetForProfile(profile));
}

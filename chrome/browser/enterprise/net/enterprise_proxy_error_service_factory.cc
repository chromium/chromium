// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"

#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/features.h"

// static
enterprise_net::EnterpriseProxyErrorService*
EnterpriseProxyErrorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<enterprise_net::EnterpriseProxyErrorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EnterpriseProxyErrorServiceFactory*
EnterpriseProxyErrorServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseProxyErrorServiceFactory> instance;
  return instance.get();
}

EnterpriseProxyErrorServiceFactory::EnterpriseProxyErrorServiceFactory()
    : ProfileKeyedServiceFactory(
          "EnterpriseProxyErrorService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              // Built only when EnterpriseProxyService exists for the profile,
              // which is gated for Isolated mode.
              .WithIsolatedMode(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(EnterpriseProxyServiceFactory::GetInstance());
}

EnterpriseProxyErrorServiceFactory::~EnterpriseProxyErrorServiceFactory() =
    default;

std::unique_ptr<KeyedService>
EnterpriseProxyErrorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!enterprise_net::IsDynamicRouteFetchingEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  // EnterpriseProxyErrorService requires EnterpriseProxyService, which is not
  // created e.g. for Isolated mode profiles while Milestone 2 is disabled.
  enterprise_net::EnterpriseProxyService* proxy_service =
      EnterpriseProxyServiceFactory::GetForProfile(profile);
  if (!proxy_service) {
    return nullptr;
  }
  return std::make_unique<enterprise_net::EnterpriseProxyErrorService>(
      proxy_service);
}

bool EnterpriseProxyErrorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool EnterpriseProxyErrorServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

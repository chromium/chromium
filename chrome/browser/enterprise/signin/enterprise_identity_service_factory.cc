// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_identity_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/signin/enterprise_identity_service.h"

namespace enterprise {

// static
EnterpriseIdentityServiceFactory*
EnterpriseIdentityServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseIdentityServiceFactory> instance;
  return instance.get();
}

// static
EnterpriseIdentityService*
EnterpriseIdentityServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<EnterpriseIdentityService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

EnterpriseIdentityServiceFactory::EnterpriseIdentityServiceFactory()
    : ProfileKeyedServiceFactory("EnterpriseIdentityService",
                                 // Only create EnterpriseIdentityService for
                                 // regular, non-Incognito profiles.
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

EnterpriseIdentityServiceFactory::~EnterpriseIdentityServiceFactory() = default;

std::unique_ptr<KeyedService>
EnterpriseIdentityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  return EnterpriseIdentityService::Create(
      IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace enterprise

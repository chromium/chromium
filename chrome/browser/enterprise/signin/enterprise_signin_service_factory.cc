// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/enterprise_signin_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"

namespace enterprise_signin {

// static
EnterpriseSigninServiceFactory* EnterpriseSigninServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseSigninServiceFactory> instance;
  return instance.get();
}

// static
EnterpriseSigninService* EnterpriseSigninServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<EnterpriseSigninService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

EnterpriseSigninServiceFactory::EnterpriseSigninServiceFactory()
    : ProfileKeyedServiceFactory("EnterpriseSigninServiceFactory",
                                 // Only create EnterpriseSigninService for
                                 // regular, non-Incognito profiles.
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

EnterpriseSigninServiceFactory::~EnterpriseSigninServiceFactory() = default;

std::unique_ptr<KeyedService>
EnterpriseSigninServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<EnterpriseSigninService>(
      Profile::FromBrowserContext(context));
}

bool EnterpriseSigninServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace enterprise_signin

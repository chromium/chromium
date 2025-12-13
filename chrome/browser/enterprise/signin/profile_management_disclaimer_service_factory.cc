
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"

#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_switches.h"

// static
ProfileManagementDisclaimerService*
ProfileManagementDisclaimerServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileManagementDisclaimerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

//  static
ProfileManagementDisclaimerServiceFactory*
ProfileManagementDisclaimerServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileManagementDisclaimerServiceFactory> instance;
  return instance.get();
}

bool ProfileManagementDisclaimerServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(switches::kEnforceManagementDisclaimer);
}

ProfileManagementDisclaimerServiceFactory::
    ProfileManagementDisclaimerServiceFactory()
    : ProfileKeyedServiceFactory("ProfileManagementDisclaimerService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DiceWebSigninInterceptorFactory::GetInstance());
}

ProfileManagementDisclaimerServiceFactory::
    ~ProfileManagementDisclaimerServiceFactory() = default;

std::unique_ptr<KeyedService> ProfileManagementDisclaimerServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<ProfileManagementDisclaimerService>(
      Profile::FromBrowserContext(context));
}

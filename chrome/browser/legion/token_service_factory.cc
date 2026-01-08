// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/legion/token_service_factory.h"

#include "chrome/browser/legion/token_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/legion/features.h"

namespace legion {

// static
legion::TokenService* TokenServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<legion::TokenService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TokenServiceFactory* TokenServiceFactory::GetInstance() {
  static base::NoDestructor<TokenServiceFactory> instance;
  return instance.get();
}

// static
ProfileSelections TokenServiceFactory::CreateProfileSelections() {
  if (!legion::TokenService::CanLegionBeEnabled()) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  return ProfileSelections::BuildForRegularProfile();
}

TokenServiceFactory::TokenServiceFactory()
    : ProfileKeyedServiceFactory("legion::TokenService",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TokenServiceFactory::~TokenServiceFactory() = default;

std::unique_ptr<KeyedService>
TokenServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(legion::TokenService::CanLegionBeEnabled());

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<legion::TokenService>(
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      profile);
}

}  // namespace legion

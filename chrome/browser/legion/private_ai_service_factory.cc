// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/legion/private_ai_service_factory.h"

#include "chrome/browser/legion/private_ai_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/legion/features.h"

namespace legion {

// static
legion::PrivateAiService* PrivateAiServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<legion::PrivateAiService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PrivateAiServiceFactory* PrivateAiServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateAiServiceFactory> instance;
  return instance.get();
}

// static
ProfileSelections PrivateAiServiceFactory::CreateProfileSelections() {
  if (!legion::PrivateAiService::CanLegionBeEnabled()) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  return ProfileSelections::BuildForRegularProfile();
}

PrivateAiServiceFactory::PrivateAiServiceFactory()
    : ProfileKeyedServiceFactory("legion::PrivateAiService",
                                 CreateProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrivateAiServiceFactory::~PrivateAiServiceFactory() = default;

std::unique_ptr<KeyedService>
PrivateAiServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(legion::PrivateAiService::CanLegionBeEnabled());

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<legion::PrivateAiService>(
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      profile);
}

}  // namespace legion

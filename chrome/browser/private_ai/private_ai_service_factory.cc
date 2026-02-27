// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service_factory.h"

#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory_impl.h"

namespace private_ai {

// static
PrivateAiService* PrivateAiServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PrivateAiService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
PrivateAiServiceFactory* PrivateAiServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateAiServiceFactory> instance;
  return instance.get();
}

PrivateAiServiceFactory::PrivateAiServiceFactory()
    : ProfileKeyedServiceFactory("private_ai::PrivateAiService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PrivateAiServiceFactory::~PrivateAiServiceFactory() = default;

std::unique_ptr<KeyedService>
PrivateAiServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!PrivateAiService::CanPrivateAiBeEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PrivateAiService>(
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      profile, std::make_unique<phosphor::BlindSignAuthFactoryImpl>());
}

}  // namespace private_ai

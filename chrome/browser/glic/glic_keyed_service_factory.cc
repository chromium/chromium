// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service_factory.h"

#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"

namespace glic {

// static
GlicKeyedService* GlicKeyedServiceFactory::GetGlicKeyedService(
    content::BrowserContext* browser_context,
    bool create) {
  return static_cast<GlicKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, create));
}

// static
GlicKeyedServiceFactory* GlicKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<GlicKeyedServiceFactory> factory;
  return factory.get();
}

GlicKeyedServiceFactory::GlicKeyedServiceFactory()
    : ProfileKeyedServiceFactory("GlicKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {
  // GlicKeyedService has an indirect dependency on the
  // RulesRegistryService through extensions::TabHelper::WebContentsDestroyed
  // when the glic web contents is destroyed.
  DependsOn(extensions::RulesRegistryService::GetFactoryInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

GlicKeyedServiceFactory::~GlicKeyedServiceFactory() = default;

bool GlicKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

std::unique_ptr<KeyedService>
GlicKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GlicKeyedService>(
      profile, IdentityManagerFactory::GetForProfile(profile),
      GlicProfileManager::GetInstance());
}

}  // namespace glic

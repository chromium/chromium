// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_keyed_service_factory.h"

#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
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
    : BrowserContextKeyedServiceFactory(
          "GlicKeyedService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ThemeServiceFactory::GetInstance());
  DependsOn(contextual_cueing::ContextualCueingServiceFactory::GetInstance());
  DependsOn(actor::ActorKeyedServiceFactory::GetInstance());
  DependsOn(subscription_eligibility::SubscriptionEligibilityServiceFactory::
                GetInstance());
}

GlicKeyedServiceFactory::~GlicKeyedServiceFactory() = default;

bool GlicKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* GlicKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(context))
             ? context
             : nullptr;
}

std::unique_ptr<KeyedService>
GlicKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GlicKeyedService>(
      profile, IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->profile_manager(), GlicProfileManager::GetInstance(),
      contextual_cueing::ContextualCueingServiceFactory::GetForProfile(profile),
      actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
}

}  // namespace glic

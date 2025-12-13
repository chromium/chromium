// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"

namespace actor {

// static
ActorKeyedService* ActorKeyedServiceFactory::GetActorKeyedService(
    content::BrowserContext* browser_context) {
  return static_cast<ActorKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
ActorKeyedServiceFactory* ActorKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<ActorKeyedServiceFactory> factory{
      base::PassKey<ActorKeyedServiceFactory>()};
  return factory.get();
}

ActorKeyedServiceFactory::ActorKeyedServiceFactory(
    base::PassKey<ActorKeyedServiceFactory>)
    : ProfileKeyedServiceFactory("ActorKeyedService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ActorKeyedServiceFactory::~ActorKeyedServiceFactory() = default;

bool ActorKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
ActorKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ActorKeyedService>(profile);
}

}  // namespace actor

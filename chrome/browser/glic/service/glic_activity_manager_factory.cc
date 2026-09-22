// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_manager_factory.h"

#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {
using actor::ActorKeyedServiceFactory;

// static
GlicActivityManagerFactory* GlicActivityManagerFactory::GetInstance() {
  static base::NoDestructor<GlicActivityManagerFactory> instance;
  return instance.get();
}

// static
GlicActivityManager* GlicActivityManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GlicActivityManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

GlicActivityManagerFactory::GlicActivityManagerFactory()
    : ProfileKeyedServiceFactory("GlicActivityManager") {
  DependsOn(ActorKeyedServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
GlicActivityManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GlicActivityManager>(
      profile, ActorKeyedServiceFactory::GetActorKeyedService(context));
}

}  // namespace glic

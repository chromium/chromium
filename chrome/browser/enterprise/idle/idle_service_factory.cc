// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace enterprise_idle {

// static
IdleService* IdleServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<IdleService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
IdleServiceFactory* IdleServiceFactory::GetInstance() {
  static base::NoDestructor<IdleServiceFactory> instance;
  return instance.get();
}

IdleServiceFactory::IdleServiceFactory()
    : ProfileKeyedServiceFactory(
          "IdleService",
          // TODO(crbug.com/40222215): Can we support Guest profiles?
          ProfileSelections::BuildForRegularProfile()) {}

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
IdleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<IdleService>(Profile::FromBrowserContext(context));
}

void IdleServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimeDeltaPref(prefs::kIdleTimeout, base::TimeDelta());
  registry->RegisterListPref(prefs::kIdleTimeoutActions);
  registry->RegisterBooleanPref(prefs::kIdleTimeoutShowBubbleOnStartup, false);
}

bool IdleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace enterprise_idle

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  return base::Singleton<IdleServiceFactory>::get();
}

IdleServiceFactory::IdleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "IdleService",
          BrowserContextDependencyManager::GetInstance()) {}

// BrowserContextKeyedServiceFactory:
KeyedService* IdleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new IdleService(Profile::FromBrowserContext(context));
}

void IdleServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(crbug.com/1316551): Use TimeDeltaPref instead.
  registry->RegisterIntegerPref(prefs::kIdleProfileCloseTimeout, 0);
}

bool IdleServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* IdleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // TODO(crbug.com/1316511): Can we support Guest profiles?
  if (profile->IsSystemProfile() || profile->IsGuestSession())
    return nullptr;

  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

}  // namespace enterprise_idle

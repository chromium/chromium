// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/core/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

// static
ChromeBrowsingDataLifetimeManagerFactory*
ChromeBrowsingDataLifetimeManagerFactory::GetInstance() {
  return base::Singleton<ChromeBrowsingDataLifetimeManagerFactory>::get();
}

// static
ChromeBrowsingDataLifetimeManager*
ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeBrowsingDataLifetimeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ChromeBrowsingDataLifetimeManagerFactory::
    ChromeBrowsingDataLifetimeManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataLifetimeManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeBrowsingDataRemoverDelegateFactory::GetInstance());
}

ChromeBrowsingDataLifetimeManagerFactory::
    ~ChromeBrowsingDataLifetimeManagerFactory() = default;

content::BrowserContext*
ChromeBrowsingDataLifetimeManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* ChromeBrowsingDataLifetimeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          browsing_data::features::kEnableBrowsingDataLifetimeManager))
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsGuestSession() && !profile->IsOffTheRecord())
    return nullptr;
  return new ChromeBrowsingDataLifetimeManager(context);
}

bool ChromeBrowsingDataLifetimeManagerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

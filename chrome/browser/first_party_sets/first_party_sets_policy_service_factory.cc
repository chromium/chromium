// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"

namespace first_party_sets {

// static
FirstPartySetsPolicyService*
FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FirstPartySetsPolicyService*>(
      FirstPartySetsPolicyServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
FirstPartySetsPolicyServiceFactory*
FirstPartySetsPolicyServiceFactory::GetInstance() {
  return base::Singleton<FirstPartySetsPolicyServiceFactory>::get();
}

FirstPartySetsPolicyServiceFactory::FirstPartySetsPolicyServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FirstPartySetsPolicyService",
          BrowserContextDependencyManager::GetInstance()) {}

FirstPartySetsPolicyServiceFactory::~FirstPartySetsPolicyServiceFactory() =
    default;

// static
void FirstPartySetsPolicyServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kFirstPartySetsEnabled, true);
  registry->RegisterDictionaryPref(kFirstPartySetsOverrides,
                                   base::DictionaryValue());
}

KeyedService* FirstPartySetsPolicyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsSystemProfile() || profile->IsGuestSession())
    return nullptr;

  if (!profile->GetPrefs()->GetBoolean(
          first_party_sets::kFirstPartySetsEnabled) ||
      !base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return nullptr;
  }

  const base::Value* policy = profile->GetPrefs()->GetDictionary(
      first_party_sets::kFirstPartySetsOverrides);
  if (!policy)
    return nullptr;

  return new FirstPartySetsPolicyService(context, policy->GetDict());
}

bool FirstPartySetsPolicyServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace first_party_sets

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"

namespace first_party_sets {

namespace {

BrowserContextKeyedServiceFactory::TestingFactory* GetTestingFactory() {
  static base::NoDestructor<BrowserContextKeyedServiceFactory::TestingFactory>
      instance;
  return instance.get();
}

}  // namespace

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

// static
const base::Value::Dict*
FirstPartySetsPolicyServiceFactory::GetOverridesPolicyForProfile(
    const Profile& profile) {
  if (profile.IsSystemProfile() || profile.IsGuestSession())
    return nullptr;

  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return nullptr;
  }

  return &profile.GetPrefs()->GetDict(
      first_party_sets::kFirstPartySetsOverrides);
}

void FirstPartySetsPolicyServiceFactory::SetTestingFactoryForTesting(
    TestingFactory test_factory) {
  *GetTestingFactory() = std::move(test_factory);
}

FirstPartySetsPolicyServiceFactory::FirstPartySetsPolicyServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FirstPartySetsPolicyService",
          BrowserContextDependencyManager::GetInstance()) {}

FirstPartySetsPolicyServiceFactory::~FirstPartySetsPolicyServiceFactory() =
    default;

content::BrowserContext*
FirstPartySetsPolicyServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* FirstPartySetsPolicyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!GetTestingFactory()->is_null()) {
    return GetTestingFactory()->Run(context).release();
  }
  Profile* profile = Profile::FromBrowserContext(context);
  // profile is guaranteed to be non-null since we create this factory with a
  // non-null `context` from the ProfileNetworkContextService.
  DCHECK(profile);
  return new FirstPartySetsPolicyService(
      context, GetOverridesPolicyForProfile(*profile));
}

bool FirstPartySetsPolicyServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

void FirstPartySetsPolicyServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kFirstPartySetsOverrides,
                                   base::Value::Dict());
}

}  // namespace first_party_sets

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace autofill {

// static
AutocompleteHistoryManager* AutocompleteHistoryManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutocompleteHistoryManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutocompleteHistoryManagerFactory*
AutocompleteHistoryManagerFactory::GetInstance() {
  return base::Singleton<AutocompleteHistoryManagerFactory>::get();
}

AutocompleteHistoryManagerFactory::AutocompleteHistoryManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AutocompleteHistoryManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

AutocompleteHistoryManagerFactory::~AutocompleteHistoryManagerFactory() {}

KeyedService* AutocompleteHistoryManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AutocompleteHistoryManager* service = new AutocompleteHistoryManager();

  auto local_storage = WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  service->Init(local_storage, profile->GetPrefs(), profile->IsOffTheRecord());

  return service;
}

content::BrowserContext*
AutocompleteHistoryManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace autofill

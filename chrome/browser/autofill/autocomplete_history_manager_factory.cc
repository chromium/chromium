// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

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
  static base::NoDestructor<AutocompleteHistoryManagerFactory> instance;
  return instance.get();
}

AutocompleteHistoryManagerFactory::AutocompleteHistoryManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutocompleteHistoryManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(WebDataServiceFactory::GetInstance());
}

AutocompleteHistoryManagerFactory::~AutocompleteHistoryManagerFactory() =
    default;

KeyedService* AutocompleteHistoryManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  AutocompleteHistoryManager* service = new AutocompleteHistoryManager();

  auto local_storage = WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  service->Init(local_storage, profile->GetPrefs(), profile->IsOffTheRecord());

  return service;
}

}  // namespace autofill

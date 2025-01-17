// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/enterprise_search_aggregator_suggestions_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/omnibox/browser/enterprise_search_aggregator_suggestions_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
EnterpriseSearchAggregatorSuggestionsService*
EnterpriseSearchAggregatorSuggestionsServiceFactory::GetForProfile(
    Profile* profile,
    bool create_if_necessary) {
  return static_cast<EnterpriseSearchAggregatorSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
EnterpriseSearchAggregatorSuggestionsServiceFactory*
EnterpriseSearchAggregatorSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<EnterpriseSearchAggregatorSuggestionsServiceFactory>
      instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
EnterpriseSearchAggregatorSuggestionsServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<EnterpriseSearchAggregatorSuggestionsService>(
      identity_manager, profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess());
}

EnterpriseSearchAggregatorSuggestionsServiceFactory::
    EnterpriseSearchAggregatorSuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "EnterpriseSearchAggregatorSuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              //   Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

EnterpriseSearchAggregatorSuggestionsServiceFactory::
    ~EnterpriseSearchAggregatorSuggestionsServiceFactory() = default;

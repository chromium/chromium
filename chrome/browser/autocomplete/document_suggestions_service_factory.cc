// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "content/public/browser/storage_partition.h"

// static
DocumentSuggestionsService* DocumentSuggestionsServiceFactory::GetForProfile(
    Profile* profile,
    bool create_if_necessary) {
  return static_cast<DocumentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
DocumentSuggestionsServiceFactory*
DocumentSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<DocumentSuggestionsServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
DocumentSuggestionsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<DocumentSuggestionsService>(
      identity_manager, profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess());
}

DocumentSuggestionsServiceFactory::DocumentSuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "DocumentSuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DocumentSuggestionsServiceFactory::~DocumentSuggestionsServiceFactory() =
    default;

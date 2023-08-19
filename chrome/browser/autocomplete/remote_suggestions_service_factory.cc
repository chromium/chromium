// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "content/public/browser/storage_partition.h"

// static
RemoteSuggestionsService* RemoteSuggestionsServiceFactory::GetForProfile(
    Profile* profile,
    bool create_if_necessary) {
  return static_cast<RemoteSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
RemoteSuggestionsServiceFactory*
RemoteSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<RemoteSuggestionsServiceFactory> instance;
  return instance.get();
}

KeyedService* RemoteSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new RemoteSuggestionsService(
      DocumentSuggestionsServiceFactory::GetForProfile(
          profile, /*create_if_necessary=*/true),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "RemoteSuggestionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DocumentSuggestionsServiceFactory::GetInstance());
}

RemoteSuggestionsServiceFactory::~RemoteSuggestionsServiceFactory() = default;

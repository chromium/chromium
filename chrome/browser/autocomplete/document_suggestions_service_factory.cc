// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  return base::Singleton<DocumentSuggestionsServiceFactory>::get();
}

KeyedService* DocumentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new DocumentSuggestionsService(
      identity_manager,
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

DocumentSuggestionsServiceFactory::DocumentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DocumentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DocumentSuggestionsServiceFactory::~DocumentSuggestionsServiceFactory() {}

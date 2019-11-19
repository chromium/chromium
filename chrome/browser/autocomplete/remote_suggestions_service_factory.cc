// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  return base::Singleton<RemoteSuggestionsServiceFactory>::get();
}

KeyedService* RemoteSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new RemoteSuggestionsService(
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RemoteSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
}

RemoteSuggestionsServiceFactory::~RemoteSuggestionsServiceFactory() {}

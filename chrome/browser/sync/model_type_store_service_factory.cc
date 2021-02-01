// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/model_type_store_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/model/model_type_store_service_impl.h"

// static
ModelTypeStoreServiceFactory* ModelTypeStoreServiceFactory::GetInstance() {
  return base::Singleton<ModelTypeStoreServiceFactory>::get();
}

// static
syncer::ModelTypeStoreService* ModelTypeStoreServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::ModelTypeStoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ModelTypeStoreServiceFactory::ModelTypeStoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ModelTypeStoreService",
          BrowserContextDependencyManager::GetInstance()) {}

ModelTypeStoreServiceFactory::~ModelTypeStoreServiceFactory() {}

KeyedService* ModelTypeStoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new syncer::ModelTypeStoreServiceImpl(profile->GetPath());
}

content::BrowserContext* ModelTypeStoreServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

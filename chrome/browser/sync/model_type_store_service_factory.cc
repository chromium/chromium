// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/model_type_store_service_factory.h"

#include "chrome/browser/profiles/profile.h"
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
    : ProfileKeyedServiceFactory(
          "ModelTypeStoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ModelTypeStoreServiceFactory::~ModelTypeStoreServiceFactory() = default;

KeyedService* ModelTypeStoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new syncer::ModelTypeStoreServiceImpl(profile->GetPath());
}

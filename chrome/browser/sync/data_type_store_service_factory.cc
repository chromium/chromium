// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/data_type_store_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/sync/model/data_type_store_service_impl.h"

// static
DataTypeStoreServiceFactory* DataTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<DataTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::DataTypeStoreService* DataTypeStoreServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::DataTypeStoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DataTypeStoreServiceFactory::DataTypeStoreServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataTypeStoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

DataTypeStoreServiceFactory::~DataTypeStoreServiceFactory() = default;

KeyedService* DataTypeStoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new syncer::DataTypeStoreServiceImpl(profile->GetPath(),
                                              profile->GetPrefs());
}

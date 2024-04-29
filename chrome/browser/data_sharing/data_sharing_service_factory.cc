// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/internal/data_sharing_service_impl.h"
#include "components/data_sharing/internal/empty_data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/sync/model/model_type_store_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace data_sharing {
// static
DataSharingServiceFactory* DataSharingServiceFactory::GetInstance() {
  static base::NoDestructor<DataSharingServiceFactory> instance;
  return instance.get();
}

// static
DataSharingService* DataSharingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DataSharingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DataSharingServiceFactory::DataSharingServiceFactory()
    : ProfileKeyedServiceFactory(
          "DataSharingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

DataSharingServiceFactory::~DataSharingServiceFactory() = default;

KeyedService* DataSharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kDataSharingFeature) ||
      context->IsOffTheRecord()) {
    return new EmptyDataSharingService();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  return new DataSharingServiceImpl(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      IdentityManagerFactory::GetForProfile(profile),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      chrome::GetChannel(),
      /*sdk_delegate=*/nullptr);
}

}  // namespace data_sharing

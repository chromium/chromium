// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/internal/data_sharing_service_impl.h"
#include "components/data_sharing/internal/empty_data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/data_sharing/public/features.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/data_sharing/android/data_sharing_ui_delegate_android.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory_bridge.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/data_sharing/desktop/data_sharing_sdk_delegate_desktop.h"
#include "chrome/browser/data_sharing/desktop/data_sharing_ui_delegate_desktop.h"
#endif

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
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DataSharingServiceFactory::~DataSharingServiceFactory() = default;

KeyedService* DataSharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kDataSharingFeature) ||
      context->IsOffTheRecord()) {
    return new EmptyDataSharingService();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<DataSharingUIDelegate> ui_delegate;
  std::unique_ptr<DataSharingSDKDelegate> sdk_delegate;

#if BUILDFLAG(IS_ANDROID)
  ui_delegate = std::make_unique<DataSharingUIDelegateAndroid>(profile);
  sdk_delegate = DataSharingSDKDelegate::CreateDelegate(
      DataSharingServiceFactoryBridge::CreateJavaSDKDelegate(profile));
#else
  ui_delegate = std::make_unique<DataSharingUIDelegateDesktop>(profile);
  sdk_delegate = std::make_unique<DataSharingSDKDelegateDesktop>(context);
#endif  // BUILDFLAG(IS_ANDROID)

  return new DataSharingServiceImpl(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      IdentityManagerFactory::GetForProfile(profile),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      chrome::GetChannel(), std::move(sdk_delegate), std::move(ui_delegate));
}

}  // namespace data_sharing

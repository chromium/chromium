// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/empty_tab_group_store_delegate.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/tab_group_store.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/saved_tab_groups/android/tab_group_store_delegate_android.h"
#endif

namespace tab_groups {
namespace {
std::unique_ptr<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>
CreateSavedTabGroupDataTypeConfiguration(Profile* profile) {
  return std::make_unique<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SAVED_TAB_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel())),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}

std::unique_ptr<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>
MaybeCreateSharedTabGroupDataTypeConfiguration(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    return nullptr;
  }

  return std::make_unique<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SHARED_TAB_GROUP_DATA,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel())),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}
}  // namespace

// static
TabGroupSyncServiceFactory* TabGroupSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupSyncServiceFactory> instance;
  return instance.get();
}

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  CHECK(!profile->IsOffTheRecord());
  return static_cast<TabGroupSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

TabGroupSyncServiceFactory::TabGroupSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "TabGroupSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = static_cast<Profile*>(context);

  auto model = std::make_unique<SavedTabGroupModel>();
  auto saved_config = CreateSavedTabGroupDataTypeConfiguration(profile);
  auto shared_config = MaybeCreateSharedTabGroupDataTypeConfiguration(profile);

  std::unique_ptr<TabGroupStoreDelegate> tab_group_store_delegate;
#if BUILDFLAG(IS_ANDROID)
  tab_group_store_delegate = std::make_unique<TabGroupStoreDelegateAndroid>();
#else
  tab_group_store_delegate = std::make_unique<EmptyTabGroupStoreDelegate>();
#endif

  auto tab_group_store =
      std::make_unique<TabGroupStore>(std::move(tab_group_store_delegate));

  return std::make_unique<TabGroupSyncServiceImpl>(
      std::move(model), std::move(saved_config), std::move(shared_config),
      std::move(tab_group_store));
}

}  // namespace tab_groups

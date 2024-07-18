// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_coordinator_impl.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/saved_tab_groups/empty_tab_group_sync_delegate.h"
#else
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#endif

namespace tab_groups {
namespace {
std::unique_ptr<SyncDataTypeConfiguration>
CreateSavedTabGroupDataTypeConfiguration(Profile* profile) {
  return std::make_unique<SyncDataTypeConfiguration>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SAVED_TAB_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel())),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
}

std::unique_ptr<SyncDataTypeConfiguration>
MaybeCreateSharedTabGroupDataTypeConfiguration(Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    return nullptr;
  }

  return std::make_unique<SyncDataTypeConfiguration>(
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
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = static_cast<Profile*>(context);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  auto metrics_logger =
      std::make_unique<TabGroupSyncMetricsLogger>(device_info_tracker);
  auto model = std::make_unique<SavedTabGroupModel>();
  auto saved_config = CreateSavedTabGroupDataTypeConfiguration(profile);
  auto shared_config = MaybeCreateSharedTabGroupDataTypeConfiguration(profile);

  auto service = std::make_unique<TabGroupSyncServiceImpl>(
      std::move(model), std::move(saved_config), std::move(shared_config),
      profile->GetPrefs(), std::move(metrics_logger));

  std::unique_ptr<TabGroupSyncDelegate> delegate;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  delegate =
      std::make_unique<TabGroupSyncDelegateDesktop>(service.get(), profile);
#else
  delegate = std::make_unique<EmptyTabGroupSyncDelegate>();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

  auto coordinator = std::make_unique<TabGroupSyncCoordinatorImpl>(
      std::move(delegate), service.get());
  service->SetCoordinator(std::move(coordinator));

  return std::move(service);
}

}  // namespace tab_groups

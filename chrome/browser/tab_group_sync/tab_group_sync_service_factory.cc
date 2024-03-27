// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"

namespace tab_groups {

// static
TabGroupSyncServiceFactory* TabGroupSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupSyncServiceFactory> instance;
  return instance.get();
}

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
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
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SAVED_TAB_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto model = std::make_unique<SavedTabGroupModel>();
  return std::make_unique<TabGroupSyncServiceImpl>(
      std::move(model), std::move(change_processor), std::move(store_factory));
}

}  // namespace tab_groups

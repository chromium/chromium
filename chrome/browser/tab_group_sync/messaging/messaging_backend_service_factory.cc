// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/messaging/messaging_backend_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/messaging/messaging_backend_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/model/data_type_store_service.h"

namespace tab_groups::messaging {

// static
MessagingBackendServiceFactory* MessagingBackendServiceFactory::GetInstance() {
  static base::NoDestructor<MessagingBackendServiceFactory> instance;
  return instance.get();
}

// static
MessagingBackendService* MessagingBackendServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<MessagingBackendService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

MessagingBackendServiceFactory::MessagingBackendServiceFactory()
    : ProfileKeyedServiceFactory(
          "MessagingBackendService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
}

MessagingBackendServiceFactory::~MessagingBackendServiceFactory() = default;

std::unique_ptr<KeyedService>
MessagingBackendServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = static_cast<Profile*>(context);

  // This service requires the data sharing feature to be enabled.
  CHECK(base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingFeature));

  auto* tab_group_sync_service =
      TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  auto service = std::make_unique<MessagingBackendServiceImpl>(
      tab_group_sync_service, data_sharing_service);

  return std::move(service);
}

}  // namespace tab_groups::messaging

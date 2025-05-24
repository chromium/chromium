// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/feature_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/collaboration/internal/messaging/configuration.h"
#include "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"
#include "components/collaboration/internal/messaging/messaging_backend_service_impl.h"
#include "components/collaboration/internal/messaging/storage/empty_messaging_backend_database.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_database_impl.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"
#include "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"
#include "components/collaboration/public/features.h"
#include "components/collaboration/public/messaging/empty_messaging_backend_service.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/model/data_type_store_service.h"

namespace collaboration::messaging {

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
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

MessagingBackendServiceFactory::~MessagingBackendServiceFactory() = default;

std::unique_ptr<KeyedService>
MessagingBackendServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  Profile* profile = static_cast<Profile*>(context);

  // This service requires the data sharing and tab group sync service features
  // to be enabled.
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled() ||
      !tab_groups::IsTabGroupSyncEnabled(profile->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          collaboration::features::kCollaborationMessaging)) {
    return std::make_unique<EmptyMessagingBackendService>();
  }

  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto tab_group_change_notifier = std::make_unique<TabGroupChangeNotifierImpl>(
      tab_group_sync_service, identity_manager);
  auto data_sharing_change_notifier =
      std::make_unique<DataSharingChangeNotifierImpl>(data_sharing_service);

  std::unique_ptr<MessagingBackendDatabase> messaging_backend_database;
  if (base::FeatureList::IsEnabled(
          collaboration::features::kCollaborationMessagingDatabase)) {
    messaging_backend_database =
        std::make_unique<MessagingBackendDatabaseImpl>(profile->GetPath());
  } else {
    messaging_backend_database =
        std::make_unique<EmptyMessagingBackendDatabase>();
  }

  auto messaging_backend_store = std::make_unique<MessagingBackendStoreImpl>(
      std::move(messaging_backend_database));
  auto instant_message_processor =
      std::make_unique<InstantMessageProcessorImpl>();

  // This configuration object allows us to control platform specific behavior.
  MessagingBackendConfiguration configuration;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  configuration.clear_chip_on_tab_selection = false;
#endif

  auto service = std::make_unique<MessagingBackendServiceImpl>(
      configuration, std::move(tab_group_change_notifier),
      std::move(data_sharing_change_notifier),
      std::move(messaging_backend_store), std::move(instant_message_processor),
      tab_group_sync_service, data_sharing_service, identity_manager);

  return std::move(service);
}

}  // namespace collaboration::messaging

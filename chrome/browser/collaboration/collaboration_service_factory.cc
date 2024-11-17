// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/collaboration_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/collaboration/internal/collaboration_service_impl.h"
#include "components/collaboration/internal/empty_collaboration_service.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/browser_context.h"

namespace collaboration {

// static
CollaborationServiceFactory* CollaborationServiceFactory::GetInstance() {
  static base::NoDestructor<CollaborationServiceFactory> instance;
  return instance.get();
}

// static
CollaborationService* CollaborationServiceFactory::GetForProfile(
    Profile* profile) {
  CHECK(profile);
  return static_cast<CollaborationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

CollaborationServiceFactory::CollaborationServiceFactory()
    : ProfileKeyedServiceFactory(
          "CollaborationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

CollaborationServiceFactory::~CollaborationServiceFactory() = default;

std::unique_ptr<KeyedService>
CollaborationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  bool isFeatureEnabled = base::FeatureList::IsEnabled(
                              data_sharing::features::kDataSharingFeature) ||
                          base::FeatureList::IsEnabled(
                              data_sharing::features::kDataSharingJoinOnly);

  if (!isFeatureEnabled || context->IsOffTheRecord()) {
    return std::make_unique<EmptyCollaborationService>();
  }

  Profile* profile = Profile::FromBrowserContext(context);

  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto* sync_service = SyncServiceFactory::GetForProfile(profile);

  auto service = std::make_unique<CollaborationServiceImpl>(
      tab_group_sync_service, data_sharing_service, identity_manager,
      sync_service);

  return service;
}

}  // namespace collaboration

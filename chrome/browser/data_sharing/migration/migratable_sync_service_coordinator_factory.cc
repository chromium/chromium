// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/migration/migratable_sync_service_coordinator_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/migration/internal/migratable_sync_service_coordinator_impl.h"
#include "content/public/browser/browser_context.h"

namespace data_sharing {

// static
MigratableSyncServiceCoordinatorFactory*
MigratableSyncServiceCoordinatorFactory::GetInstance() {
  static base::NoDestructor<MigratableSyncServiceCoordinatorFactory> instance;
  return instance.get();
}

// static
data_sharing::MigratableSyncServiceCoordinator*
MigratableSyncServiceCoordinatorFactory::GetForProfile(Profile* profile) {
  return static_cast<data_sharing::MigratableSyncServiceCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

MigratableSyncServiceCoordinatorFactory::
    MigratableSyncServiceCoordinatorFactory()
    : ProfileKeyedServiceFactory(
          "MigratableSyncServiceCoordinator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

MigratableSyncServiceCoordinatorFactory::
    ~MigratableSyncServiceCoordinatorFactory() = default;

std::unique_ptr<KeyedService>
MigratableSyncServiceCoordinatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<data_sharing::MigratableSyncServiceCoordinatorImpl>(
      profile->GetPath());
}

}  // namespace data_sharing

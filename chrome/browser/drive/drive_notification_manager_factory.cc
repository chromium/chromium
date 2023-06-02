// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_notification_manager_factory.h"

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/drive/drive_notification_manager.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/sync/base/command_line_switches.h"

namespace drive {
namespace {

constexpr char kDriveFcmSenderId[] = "947318989803";

invalidation::InvalidationService* GetInvalidationService(Profile* profile) {
  if (!invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          profile)) {
    return nullptr;
  }
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
             profile)
      ->GetInvalidationServiceForCustomSender(kDriveFcmSenderId);
}

}  // namespace

// static
DriveNotificationManager*
DriveNotificationManagerFactory::FindForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DriveNotificationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
DriveNotificationManager*
DriveNotificationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!syncer::IsSyncAllowedByFlag())
    return nullptr;
  if (!GetInvalidationService(Profile::FromBrowserContext(context))) {
    // Do not create a DriveNotificationManager for |context|s that do not
    // support invalidation.
    return nullptr;
  }

  return static_cast<DriveNotificationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DriveNotificationManagerFactory*
DriveNotificationManagerFactory::GetInstance() {
  static base::NoDestructor<DriveNotificationManagerFactory> instance;
  return instance.get();
}

DriveNotificationManagerFactory::DriveNotificationManagerFactory()
    : ProfileKeyedServiceFactory(
          "DriveNotificationManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

DriveNotificationManagerFactory::~DriveNotificationManagerFactory() = default;

KeyedService* DriveNotificationManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DriveNotificationManager(
      GetInvalidationService(Profile::FromBrowserContext(context)));
}

}  // namespace drive

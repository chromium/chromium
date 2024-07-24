// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_notification_manager_factory.h"

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/drive_notification_manager.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"

namespace drive {
namespace {

constexpr char kDriveFcmSenderId[] = "947318989803";

invalidation::InvalidationService* GetInvalidationService(Profile* profile) {
  auto* profile_invalidation_factory =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile);
  if (!profile_invalidation_factory) {
    return nullptr;
  }
  auto invalidation_service_or_listener =
      profile_invalidation_factory->GetInvalidationServiceOrListener(
          kDriveFcmSenderId, /*project_id=*/"");
  CHECK(std::holds_alternative<invalidation::InvalidationService*>(
      invalidation_service_or_listener))
      << "Drive does not support InvalidationListener and must be used with "
         "InvalidationService";

  return std::get<invalidation::InvalidationService*>(
      invalidation_service_or_listener);
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
DriveNotificationManager* DriveNotificationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
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
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
}

DriveNotificationManagerFactory::~DriveNotificationManagerFactory() = default;

std::unique_ptr<KeyedService>
DriveNotificationManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DriveNotificationManager>(
      GetInvalidationService(Profile::FromBrowserContext(context)));
}

}  // namespace drive

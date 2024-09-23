// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_factory.h"

StorageNotificationServiceFactory::StorageNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "StorageNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}
StorageNotificationServiceFactory::~StorageNotificationServiceFactory() =
    default;

// static
StorageNotificationServiceImpl*
StorageNotificationServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<StorageNotificationServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
StorageNotificationServiceFactory*
StorageNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<StorageNotificationServiceFactory> instance;
  return instance.get();
}

// static
KeyedService* StorageNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return BuildInstanceFor(browser_context).release();
}

std::unique_ptr<KeyedService>
StorageNotificationServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  return std::make_unique<StorageNotificationServiceImpl>();
}

bool StorageNotificationServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

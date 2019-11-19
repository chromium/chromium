// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

StorageNotificationServiceFactory::StorageNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "StorageNotificationService",
          BrowserContextDependencyManager::GetInstance()) {}
StorageNotificationServiceFactory::~StorageNotificationServiceFactory() {}

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
  return base::Singleton<StorageNotificationServiceFactory>::get();
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

content::BrowserContext*
StorageNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

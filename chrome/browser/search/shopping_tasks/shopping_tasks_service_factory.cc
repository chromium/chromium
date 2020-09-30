// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/shopping_tasks/shopping_tasks_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
ShoppingTasksService* ShoppingTasksServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ShoppingTasksService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ShoppingTasksServiceFactory* ShoppingTasksServiceFactory::GetInstance() {
  return base::Singleton<ShoppingTasksServiceFactory>::get();
}

ShoppingTasksServiceFactory::ShoppingTasksServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ShoppingTasksService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

ShoppingTasksServiceFactory::~ShoppingTasksServiceFactory() = default;

KeyedService* ShoppingTasksServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new ShoppingTasksService(url_loader_factory,
                                  Profile::FromBrowserContext(context),
                                  g_browser_process->GetApplicationLocale());
}

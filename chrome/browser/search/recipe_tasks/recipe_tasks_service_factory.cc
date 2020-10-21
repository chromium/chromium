// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/recipe_tasks/recipe_tasks_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/recipe_tasks/recipe_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
RecipeTasksService* RecipeTasksServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<RecipeTasksService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RecipeTasksServiceFactory* RecipeTasksServiceFactory::GetInstance() {
  return base::Singleton<RecipeTasksServiceFactory>::get();
}

RecipeTasksServiceFactory::RecipeTasksServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RecipeTasksService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

RecipeTasksServiceFactory::~RecipeTasksServiceFactory() = default;

KeyedService* RecipeTasksServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new RecipeTasksService(url_loader_factory,
                                Profile::FromBrowserContext(context),
                                g_browser_process->GetApplicationLocale());
}

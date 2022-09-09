// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/recipes/recipes_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
RecipesService* RecipesServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<RecipesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RecipesServiceFactory* RecipesServiceFactory::GetInstance() {
  return base::Singleton<RecipesServiceFactory>::get();
}

RecipesServiceFactory::RecipesServiceFactory()
    : ProfileKeyedServiceFactory("RecipesService") {
  DependsOn(CookieSettingsFactory::GetInstance());
}

RecipesServiceFactory::~RecipesServiceFactory() = default;

KeyedService* RecipesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return new RecipesService(url_loader_factory,
                            Profile::FromBrowserContext(context),
                            g_browser_process->GetApplicationLocale());
}

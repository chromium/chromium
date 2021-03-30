// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/drive/drive_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

DriveService* DriveServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DriveService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DriveServiceFactory* DriveServiceFactory::GetInstance() {
  return base::Singleton<DriveServiceFactory>::get();
}

DriveServiceFactory::DriveServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "DriveService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

DriveServiceFactory::~DriveServiceFactory() = default;

KeyedService* DriveServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new DriveService(url_loader_factory,
                          IdentityManagerFactory::GetForProfile(
                              Profile::FromBrowserContext(context)));
}

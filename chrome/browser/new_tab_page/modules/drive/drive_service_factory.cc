// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/drive/drive_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/new_tab_page/modules/drive/drive_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
    : ProfileKeyedServiceFactory("DriveService") {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DriveServiceFactory::~DriveServiceFactory() = default;

KeyedService* DriveServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto* profile = Profile::FromBrowserContext(context);
  return new DriveService(
      url_loader_factory, IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->GetApplicationLocale(), profile->GetPrefs());
}

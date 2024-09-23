// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

DriveService* DriveServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DriveService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DriveServiceFactory* DriveServiceFactory::GetInstance() {
  static base::NoDestructor<DriveServiceFactory> instance;
  return instance.get();
}

DriveServiceFactory::DriveServiceFactory()
    : ProfileKeyedServiceFactory(
          "DriveService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
}

DriveServiceFactory::~DriveServiceFactory() = default;

std::unique_ptr<KeyedService>
DriveServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<DriveService>(
      url_loader_factory, IdentityManagerFactory::GetForProfile(profile),
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile),
      g_browser_process->GetApplicationLocale(), profile->GetPrefs());
}

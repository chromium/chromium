// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#else
#include "chrome/browser/supervised_user/desktop/supervised_user_service_platform_delegate.h"
#endif

namespace supervised_user {

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SupervisedUserUrlFilteringServiceFactory*
SupervisedUserUrlFilteringServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserUrlFilteringServiceFactory> instance;
  return instance.get();
}

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory()
    : ProfileKeyedServiceFactory("SupervisedUserUrlFilteringService",
                                 BuildProfileSelectionsForRegularAndGuest()) {
  // Temporary dependency on the SupervisedUserService instance to allow
  // migration of legacy SupervisedUserURLFilter methods that are called through
  // the SupervisedUserService: service_->GetURLFilter()->Method(). Remove once
  // all callers are migrated to the SupervisedUserUrlFilteringService.
  // TODO(crbug.com/469336110): Remove this dependency after migration.
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

SupervisedUserUrlFilteringServiceFactory::
    ~SupervisedUserUrlFilteringServiceFactory() = default;

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  SupervisedUserServicePlatformDelegate platform_delegate(*profile);

  return std::make_unique<SupervisedUserUrlFilteringService>(
      CHECK_DEREF(SupervisedUserServiceFactory::GetForProfile(profile)),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          g_browser_process->device_parental_controls(),
          std::make_unique<KidsChromeManagementURLCheckerClient>(
              context->GetDefaultStoragePartition()
                  ->GetURLLoaderFactoryForBrowserProcess(),
              platform_delegate.GetCountryCode(),
              platform_delegate.GetChannel())));
}

}  // namespace supervised_user

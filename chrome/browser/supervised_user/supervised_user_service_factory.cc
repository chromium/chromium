// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service_factory.h"

#include "base/functional/bind.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_content_filters_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#else
#include "chrome/browser/supervised_user/desktop/supervised_user_service_platform_delegate.h"
#endif

class FilterDelegateImpl
    : public supervised_user::SupervisedUserURLFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override {
    return supervised_user::IsSupportedChromeExtensionURL(url);
  }
};

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<supervised_user::SupervisedUserService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetForProfile(Profile::FromBrowserContext(context));
}

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<supervised_user::SupervisedUserService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SupervisedUserServiceFactory* SupervisedUserServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserServiceFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<KeyedService> SupervisedUserServiceFactory::BuildInstanceFor(
    Profile* profile) {
  std::unique_ptr<SupervisedUserServicePlatformDelegate> platform_delegate =
      std::make_unique<SupervisedUserServicePlatformDelegate>(*profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  return std::make_unique<supervised_user::SupervisedUserService>(
      identity_manager, url_loader_factory, *profile->GetPrefs(),
      *SupervisedUserSettingsServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()),
#if BUILDFLAG(IS_ANDROID)
      SupervisedUserContentFiltersServiceFactory::GetInstance()->GetForKey(
          profile->GetProfileKey()),
#else
      nullptr,
#endif  // BUILDFLAG(IS_ANDROID)
      SyncServiceFactory::GetInstance()->GetForProfile(profile),
      std::make_unique<supervised_user::SupervisedUserURLFilter>(
          *profile->GetPrefs(), std::make_unique<FilterDelegateImpl>(),
          std::make_unique<
              supervised_user::KidsChromeManagementURLCheckerClient>(
              identity_manager, url_loader_factory, *profile->GetPrefs(),
              platform_delegate->GetCountryCode(),
              platform_delegate->GetChannel())),
      std::move(platform_delegate)
#if BUILDFLAG(IS_ANDROID)
          ,
      base::BindRepeating(
          &supervised_user::ContentFiltersObserverBridge::Create)
#endif  // BUILDFLAG(IS_ANDROID)
  );
}

SupervisedUserServiceFactory::SupervisedUserServiceFactory()
    : ProfileKeyedServiceFactory(
          "SupervisedUserService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
  DependsOn(SupervisedUserContentFiltersServiceFactory::GetInstance());
}

SupervisedUserServiceFactory::~SupervisedUserServiceFactory() = default;

std::unique_ptr<KeyedService>
SupervisedUserServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
